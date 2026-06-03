
#define STB_IMAGE_IMPLEMENTATION
#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/videodev2.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <dirent.h>
#include <sys/stat.h>
#include "stb_image.h"

// Define function pointers for modern GL functions not in standard headers (for texture loading)
typedef void (APIENTRYP PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);

PFNGLGENTEXTURESPROC my_glGenTextures;
PFNGLBINDTEXTUREPROC my_glBindTexture;
PFNGLTEXIMAGE2DPROC  my_glTexImage2D;
PFNGLDELETETEXTURESPROC my_glDeleteTextures;

void load_gl_pointers() {
    // Manually fetch addresses from the driver - try different methods
    // First try using glXGetProcAddress if available (X11/GLX context)
    // The function pointers are actually available directly in the OpenGL library
    // For GLUT applications, these functions should be available directly
    my_glGenTextures = glGenTextures;
    my_glBindTexture = glBindTexture;
    my_glTexImage2D  = glTexImage2D;
    my_glDeleteTextures = glDeleteTextures;
}

// Texture cache entry structure
#define MAX_CACHED_TEXTURES 100
typedef struct {
    char path[512];
    GLuint texture_id;
    int width;
    int height;
    int loaded;     // Whether this cache entry is populated
} TextureCacheEntry;

TextureCacheEntry texture_cache[MAX_CACHED_TEXTURES];
int texture_cache_size = 0;

// Initialize texture cache
void init_texture_cache() {
    for (int i = 0; i < MAX_CACHED_TEXTURES; i++) {
        texture_cache[i].loaded = 0;
        texture_cache[i].texture_id = 0;
        texture_cache[i].path[0] = '\0';
        texture_cache[i].width = 0;
        texture_cache[i].height = 0;
    }
    texture_cache_size = 0;
}

// Find a texture in the cache
int find_texture_in_cache(const char* path) {
    for (int i = 0; i < texture_cache_size && i < MAX_CACHED_TEXTURES; i++) {
        if (texture_cache[i].loaded && strcmp(texture_cache[i].path, path) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Add a texture to the cache
int add_texture_to_cache(const char* path, GLuint texture_id, int width, int height) {
    // Check if texture is already in cache (avoid duplicates)
    int existing_index = find_texture_in_cache(path);
    if (existing_index != -1) {
        return existing_index;
    }
    
    // If cache is full, we could implement a replacement strategy
    // For now, just return if cache is full
    if (texture_cache_size >= MAX_CACHED_TEXTURES) {
        // Optional: Implement LRU or other cache replacement strategy
        return -1;
    }
    
    // Add to cache
    strncpy(texture_cache[texture_cache_size].path, path, sizeof(texture_cache[texture_cache_size].path) - 1);
    texture_cache[texture_cache_size].path[sizeof(texture_cache[texture_cache_size].path) - 1] = '\0';
    texture_cache[texture_cache_size].texture_id = texture_id;
    texture_cache[texture_cache_size].width = width;
    texture_cache[texture_cache_size].height = height;
    texture_cache[texture_cache_size].loaded = 1;
    
    int index = texture_cache_size;
    texture_cache_size++;
    
    return index;
}

// Load texture from file using stb_image
GLuint load_texture_from_file(const char* path) {
    // Check if texture is already in cache
    int cache_index = find_texture_in_cache(path);
    if (cache_index != -1) {
        // Texture is in cache, use it
        return texture_cache[cache_index].texture_id;
    }

    // Load image using stb_image
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 4); // Force RGBA
    if (!data) {
        printf("Failed to load image: %s\n", path);
        return 0; // Return 0 for failed texture
    }
    printf("Loaded image %s (%dx%d)\n", path, width, height);

    // Create OpenGL texture
    GLuint texture_id;
    my_glGenTextures(1, &texture_id);
    my_glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set filtering parameters (required to see the texture)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload raw pixel data to GPU
    my_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    printf("Texture generated successfully with ID: %u\n", texture_id);

    // Add to cache
    add_texture_to_cache(path, texture_id, width, height);

    return texture_id;
}

// Definition for the renderable shape object provided by the model
typedef struct {
    int id;
    char type[32];  // Increased size
    float x, y, z;
    float width, height, depth;  // Separate dimensions
    float color[3];
    float alpha;
    char label[64];  // Added label
    int active;      // Added active flag
    float rotation_x, rotation_y, rotation_z; // Rotation angles in degrees
    float scale_x, scale_y, scale_z; // Scale factors
    // Texture fields for module shapes
    char texture_src[256];    // path from module SHAPE command
    GLuint texture_id;        // OpenGL texture ID
    int texture_loaded;       // flag to indicate if texture has been loaded
    int texture_loading;      // flag to prevent multiple loading attempts
} Shape;

// Coordinate transformation utilities
typedef struct {
    float x, y;
} Point2D;

typedef struct {
    float x, y, z;
} Point3D;

// Convert window coordinates to OpenGL coordinates (Y flip)
static inline int convert_y_to_opengl_coords(int window_height, int y) {
    return window_height - y;
}

// 2D to 3D coordinate mapping for shapes
static inline Point3D map_2d_to_3d_coords(float x, float y, float z, int canvas_width, int canvas_height) {
    Point3D result;
    result.x = (x / (float)canvas_width) * 10.0f - 5.0f;
    result.y = (y / (float)canvas_height) * 10.0f - 5.0f;
    result.z = z;
    return result;
}

// 3D transformation utilities
static inline void scale_3d_dimensions(float width, float height, float depth, float* size_x, float* size_y, float* size_z) {
    *size_x = width / 10.0f;
    *size_y = height / 10.0f;
    *size_z = depth / 10.0f;
}

// Vector math utilities
static inline float vector_length_2d(float x, float y) {
    return sqrtf(x*x + y*y);
}

static inline float vector_length_3d(float x, float y, float z) {
    return sqrtf(x*x + y*y + z*z);
}

static inline float distance_2d(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

static inline float distance_3d(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

// Matrix utilities for 3D transformations
typedef float Matrix4x4[16];

// Create identity matrix
static inline void matrix_identity(Matrix4x4 m) {
    for (int i = 0; i < 16; i++) {
        m[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Diagonal elements are 1
    }
}

// Basic trigonometric helpers
static inline float degrees_to_radians(float degrees) {
    return degrees * M_PI / 180.0f;
}

static inline float radians_to_degrees(float radians) {
    return radians * 180.0f / M_PI;
}

// Shape rendering utility functions

// Render 2D shapes
static inline void render_2d_shape(Shape* s) {
    // Check if shape has texture and load it if necessary
    if (s->texture_src[0] != '\0') {  // Check if texture source provided
        if (!s->texture_loaded && !s->texture_loading) {
            // Mark as loading to prevent race conditions
            s->texture_loading = 1;
            s->texture_id = load_texture_from_file(s->texture_src);
            if (s->texture_id != 0) {
                s->texture_loaded = 1;
                printf("Loaded texture for shape: %s from %s\n", s->label, s->texture_src);
            }
            s->texture_loading = 0;
        }
    }
    
    // Set color and alpha
    glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
    
    if (strcmp(s->type, "SQUARE") == 0 || strcmp(s->type, "RECT") == 0) {
        // Check if we have a loaded texture to apply
        if (s->texture_loaded && s->texture_id != 0) {
            // Save the current OpenGL state
            glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
            
            // Enable texture and blending
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // Bind the texture
            glBindTexture(GL_TEXTURE_2D, s->texture_id);
            
            // Set texture parameters for proper rendering
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            // Apply scaling in 2D space only if scale differs from default to maintain backward compatibility
            if (s->scale_x != 1.0f || s->scale_y != 1.0f) {
                float scaled_width = s->width * s->scale_x;
                float scaled_height = s->height * s->scale_y;
                
                glBegin(GL_QUADS);
                // Apply texture coordinates to the quad
                glTexCoord2f(0.0f, 0.0f); glVertex2f(s->x - scaled_width / 2, s->y - scaled_height / 2);  // Bottom-left
                glTexCoord2f(1.0f, 0.0f); glVertex2f(s->x + scaled_width / 2, s->y - scaled_height / 2);  // Bottom-right
                glTexCoord2f(1.0f, 1.0f); glVertex2f(s->x + scaled_width / 2, s->y + scaled_height / 2);  // Top-right
                glTexCoord2f(0.0f, 1.0f); glVertex2f(s->x - scaled_width / 2, s->y + scaled_height / 2);  // Top-left
                glEnd();
            } else {
                // For backward compatibility - render without scaling when values are defaults
                glBegin(GL_QUADS);
                // Apply texture coordinates to the quad
                glTexCoord2f(0.0f, 0.0f); glVertex2f(s->x - s->width / 2, s->y - s->height / 2);  // Bottom-left
                glTexCoord2f(1.0f, 0.0f); glVertex2f(s->x + s->width / 2, s->y - s->height / 2);  // Bottom-right
                glTexCoord2f(1.0f, 1.0f); glVertex2f(s->x + s->width / 2, s->y + s->height / 2);  // Top-right
                glTexCoord2f(0.0f, 1.0f); glVertex2f(s->x - s->width / 2, s->y + s->height / 2);  // Top-left
                glEnd();
            }
            
            // Restore the previous OpenGL state
            glPopAttrib();
        } else {
            // Apply scaling in 2D space only if scale differs from default to maintain backward compatibility
            if (s->scale_x != 1.0f || s->scale_y != 1.0f) {
                float scaled_width = s->width * s->scale_x;
                float scaled_height = s->height * s->scale_y;
                
                glBegin(GL_QUADS);
                glVertex2f(s->x - scaled_width / 2, s->y - scaled_height / 2);
                glVertex2f(s->x + scaled_width / 2, s->y - scaled_height / 2);
                glVertex2f(s->x + scaled_width / 2, s->y + scaled_height / 2);
                glVertex2f(s->x - scaled_width / 2, s->y + scaled_height / 2);
                glEnd();
            } else {
                // For backward compatibility - render without scaling when values are defaults
                glBegin(GL_QUADS);
                glVertex2f(s->x - s->width / 2, s->y - s->height / 2);
                glVertex2f(s->x + s->width / 2, s->y - s->height / 2);
                glVertex2f(s->x + s->width / 2, s->y + s->height / 2);
                glVertex2f(s->x - s->width / 2, s->y + s->height / 2);
                glEnd();
            }
        }
    } else if (strcmp(s->type, "CIRCLE") == 0) {
        // Apply scaling in 2D space only if scale differs from default to maintain backward compatibility
        if (s->scale_x != 1.0f || s->scale_y != 1.0f) {
            float scaled_width = s->width * s->scale_x;
            float scaled_height = s->height * s->scale_y;
            
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(s->x, s->y);
            for (int j = 0; j <= 20; j++) {
                float angle = j * (2.0f * 3.14159f / 20);
                float dx = scaled_width / 2 * cos(angle);
                float dy = scaled_height / 2 * sin(angle);
                glVertex2f(s->x + dx, s->y + dy);
            }
            glEnd();
        } else {
            // For backward compatibility - render without scaling when values are defaults
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(s->x, s->y);
            for (int j = 0; j <= 20; j++) {
                float angle = j * (2.0f * 3.14159f / 20);
                float dx = s->width / 2 * cos(angle);
                float dy = s->height / 2 * sin(angle);
                glVertex2f(s->x + dx, s->y + dy);
            }
            glEnd();
        }
    } else if (strcmp(s->type, "TRIANGLE") == 0) {
        // Apply scaling in 2D space only if scale differs from default to maintain backward compatibility
        if (s->scale_x != 1.0f || s->scale_y != 1.0f) {
            float scaled_width = s->width * s->scale_x;
            float scaled_height = s->height * s->scale_y;
            
            glBegin(GL_TRIANGLES);
            // Calculate the three vertices of the triangle
            // For an equilateral triangle centered at (x, y)
            float half_width = scaled_width / 2.0f;
            float half_height = scaled_height / 2.0f;
            
            // Vertex 1: Top vertex
            glVertex2f(s->x, s->y + half_height);
            // Vertex 2: Bottom left
            glVertex2f(s->x - half_width, s->y - half_height);
            // Vertex 3: Bottom right
            glVertex2f(s->x + half_width, s->y - half_height);
            glEnd();
        } else {
            // For backward compatibility - render without scaling when values are defaults
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

// Render 3D rectangular prism with optional texture support
static inline void render_3d_rect_prism(float x_3d, float y_3d, float z_3d, float size_x, float size_y, float size_z) {
    glPushMatrix();
    glTranslatef(x_3d, y_3d, z_3d); // Position in proper 3D space
    
    // Draw a rectangular prism 
    glBegin(GL_QUADS);
    // Front face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-size_x/2, -size_y/2, size_z/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(size_x/2, -size_y/2, size_z/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(size_x/2, size_y/2, size_z/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-size_x/2, size_y/2, size_z/2);
    
    // Back face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-size_x/2, -size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-size_x/2, size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(size_x/2, size_y/2, -size_z/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(size_x/2, -size_y/2, -size_z/2);
    
    // Top face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-size_x/2, size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-size_x/2, size_y/2, size_z/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(size_x/2, size_y/2, size_z/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(size_x/2, size_y/2, -size_z/2);
    
    // Bottom face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-size_x/2, -size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(size_x/2, -size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(size_x/2, -size_y/2, size_z/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-size_x/2, -size_y/2, size_z/2);
    
    // Left face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-size_x/2, -size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-size_x/2, -size_y/2, size_z/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-size_x/2, size_y/2, size_z/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-size_x/2, size_y/2, -size_z/2);
    
    // Right face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(size_x/2, -size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(size_x/2, size_y/2, -size_z/2);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(size_x/2, size_y/2, size_z/2);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(size_x/2, -size_y/2, size_z/2);
    glEnd();
    glPopMatrix();
}

// Render 3D triangle pyramid
static inline void render_3d_triangle_pyramid(float x_3d, float y_3d, float z_3d, float avg_size) {
    glPushMatrix();
    glTranslatef(x_3d, y_3d, z_3d); // Position in proper 3D space
    
    // Draw a triangle pyramid with proper 3D positioning
    glBegin(GL_TRIANGLES);
    // Front face
    glVertex3f(0.0f, avg_size/10.0f, avg_size/20.0f);  // Top
    glVertex3f(-avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);  // Bottom left
    glVertex3f(avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);   // Bottom right
    
    // Right face
    glVertex3f(0.0f, avg_size/10.0f, avg_size/20.0f);  // Top
    glVertex3f(avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);   // Bottom right
    glVertex3f(0.0f, avg_size/20.0f, avg_size/10.0f);  // Top back (corrected)
    
    // Left face
    glVertex3f(0.0f, avg_size/10.0f, avg_size/20.0f);  // Top
    glVertex3f(0.0f, avg_size/20.0f, avg_size/10.0f);  // Top back
    glVertex3f(-avg_size/20.0f, -avg_size/20.0f, avg_size/20.0f);  // Bottom left
    glEnd();
    glPopMatrix();
}

// Render 3D cylinder (for circles in 3D)
static inline void render_3d_cylinder(float x_3d, float y_3d, float z_3d, float radius, float height) {
    glPushMatrix();
    glTranslatef(x_3d, y_3d, z_3d); // Position in proper 3D space
    
    int num_segments = 20;
    
    // Draw the cylinder sides
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= num_segments; j++) {
        float angle = j * 2.0f * 3.14159f / num_segments;
        float x_pos = radius * cos(angle);
        float z_pos = radius * sin(angle);
        glVertex3f(x_pos, height/2, z_pos);  // Top edge
        glVertex3f(x_pos, -height/2, z_pos); // Bottom edge
    }
    glEnd();
    
    // Draw top disc
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, height/2, 0.0f); // Center of the top disc
    for (int j = 0; j <= num_segments; j++) {
        float angle = j * 2.0f * 3.14159f / num_segments;
        float x_pos = radius * cos(angle);
        float z_pos = radius * sin(angle);
        glVertex3f(x_pos, height/2, z_pos);
    }
    glEnd();
    
    // Draw bottom disc
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, -height/2, 0.0f); // Center of the bottom disc
    for (int j = num_segments; j >= 0; j--) {
        float angle = j * 2.0f * 3.14159f / num_segments;
        float x_pos = radius * cos(angle);
        float z_pos = radius * sin(angle);
        glVertex3f(x_pos, -height/2, z_pos);
    }
    glEnd();
    glPopMatrix();
}

// Render 3D shape based on type
static inline void render_3d_shape(Shape* s, int canvas_width, int canvas_height) {
    // Check if shape has texture and load it if necessary
    if (s->texture_src[0] != '\0') {  // Check if texture source provided
        if (!s->texture_loaded && !s->texture_loading) {
            // Mark as loading to prevent race conditions
            s->texture_loading = 1;
            s->texture_id = load_texture_from_file(s->texture_src);
            if (s->texture_id != 0) {
                s->texture_loaded = 1;
                printf("Loaded texture for 3D shape: %s from %s\n", s->label, s->texture_src);
            }
            s->texture_loading = 0;
        }
    }
    
    if (strcmp(s->type, "SQUARE") == 0 || strcmp(s->type, "RECT") == 0) {
        // Map the 2D coordinates from the module to 3D space with proper depth
        Point3D pos_3d = map_2d_to_3d_coords(s->x, s->y, s->z, canvas_width, canvas_height);
        
        // Use width and height for 3D scaling
        float size_x, size_y, size_z;
        scale_3d_dimensions(s->width, s->height, s->depth, &size_x, &size_y, &size_z);
        
        // Apply transformations only if they differ from default values to maintain backward compatibility
        if (s->rotation_x != 0.0f || s->rotation_y != 0.0f || s->rotation_z != 0.0f || 
            s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) {
            glPushMatrix();
            // Apply transformations: translate, rotate, scale
            glTranslatef(pos_3d.x, pos_3d.y, pos_3d.z);
            if (s->rotation_x != 0.0f) glRotatef(s->rotation_x, 1.0f, 0.0f, 0.0f); // Rotate around X-axis
            if (s->rotation_y != 0.0f) glRotatef(s->rotation_y, 0.0f, 1.0f, 0.0f); // Rotate around Y-axis
            if (s->rotation_z != 0.0f) glRotatef(s->rotation_z, 0.0f, 0.0f, 1.0f); // Rotate around Z-axis
            if (s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) glScalef(s->scale_x, s->scale_y, s->scale_z); // Apply scaling
            
            // Check if we have a loaded texture to apply to the 3D rectangular prism
            if (s->texture_loaded && s->texture_id != 0) {
                // Save the current OpenGL state
                glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
                
                // Enable texture and blending
                glEnable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Bind the texture
                glBindTexture(GL_TEXTURE_2D, s->texture_id);
                
                // Set texture parameters for proper rendering
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                
                // Render textured 3D rectangular prism
                glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
                render_3d_rect_prism(0.0f, 0.0f, 0.0f, size_x, size_y, size_z);
                
                // Restore the previous OpenGL state
                glPopAttrib();
            } else {
                // Render without texture
                glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
                render_3d_rect_prism(0.0f, 0.0f, 0.0f, size_x, size_y, size_z);
            }
            glPopMatrix();
        } else {
            // For backward compatibility - render without transformations when values are defaults
            
            // Check if we have a loaded texture to apply to the 3D rectangular prism
            if (s->texture_loaded && s->texture_id != 0) {
                // Save the current OpenGL state
                glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
                
                // Enable texture and blending
                glEnable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Bind the texture
                glBindTexture(GL_TEXTURE_2D, s->texture_id);
                
                // Set texture parameters for proper rendering
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                
                // Render textured 3D rectangular prism
                glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
                render_3d_rect_prism(pos_3d.x, pos_3d.y, pos_3d.z, size_x, size_y, size_z);
                
                // Restore the previous OpenGL state
                glPopAttrib();
            } else {
                // Render without texture
                glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
                render_3d_rect_prism(pos_3d.x, pos_3d.y, pos_3d.z, size_x, size_y, size_z);
            }
        }
    } else if (strcmp(s->type, "TRIANGLE") == 0) {
        // Map the 2D coordinates from the module to 3D space with proper depth
        Point3D pos_3d = map_2d_to_3d_coords(s->x, s->y, s->z, canvas_width, canvas_height);
        // Use width and height for 3D scaling
        float avg_size = (s->width + s->height) / 2.0f;
        
        // Apply transformations only if they differ from default values to maintain backward compatibility
        if (s->rotation_x != 0.0f || s->rotation_y != 0.0f || s->rotation_z != 0.0f || 
            s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) {
            glPushMatrix();
            // Apply transformations: translate, rotate, scale
            glTranslatef(pos_3d.x, pos_3d.y, pos_3d.z);
            if (s->rotation_x != 0.0f) glRotatef(s->rotation_x, 1.0f, 0.0f, 0.0f); // Rotate around X-axis
            if (s->rotation_y != 0.0f) glRotatef(s->rotation_y, 0.0f, 1.0f, 0.0f); // Rotate around Y-axis
            if (s->rotation_z != 0.0f) glRotatef(s->rotation_z, 0.0f, 0.0f, 1.0f); // Rotate around Z-axis
            if (s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) glScalef(s->scale_x, s->scale_y, s->scale_z); // Apply scaling
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_3d_triangle_pyramid(0.0f, 0.0f, 0.0f, avg_size);
            glPopMatrix();
        } else {
            // For backward compatibility - render without transformations when values are defaults
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_3d_triangle_pyramid(pos_3d.x, pos_3d.y, pos_3d.z, avg_size);
        }
    } else if (strcmp(s->type, "CIRCLE") == 0) {
        // Map the 2D coordinates from the module to proper 3D space
        Point3D pos_3d = map_2d_to_3d_coords(s->x, s->y, s->z, canvas_width, canvas_height);
        // Use width and height for 3D scaling
        float radius = s->width / 20.0f; // Scale down for 3D space
        float height = s->height / 20.0f; // Use height parameter for 3D
        
        // Apply transformations only if they differ from default values to maintain backward compatibility
        if (s->rotation_x != 0.0f || s->rotation_y != 0.0f || s->rotation_z != 0.0f || 
            s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) {
            glPushMatrix();
            // Apply transformations: translate, rotate, scale
            glTranslatef(pos_3d.x, pos_3d.y, pos_3d.z);
            if (s->rotation_x != 0.0f) glRotatef(s->rotation_x, 1.0f, 0.0f, 0.0f); // Rotate around X-axis
            if (s->rotation_y != 0.0f) glRotatef(s->rotation_y, 0.0f, 1.0f, 0.0f); // Rotate around Y-axis
            if (s->rotation_z != 0.0f) glRotatef(s->rotation_z, 0.0f, 0.0f, 1.0f); // Rotate around Z-axis
            if (s->scale_x != 1.0f || s->scale_y != 1.0f || s->scale_z != 1.0f) glScalef(s->scale_x, s->scale_y, s->scale_z); // Apply scaling
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_3d_cylinder(0.0f, 0.0f, 0.0f, radius, height);
            glPopMatrix();
        } else {
            // For backward compatibility - render without transformations when values are defaults
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_3d_cylinder(pos_3d.x, pos_3d.y, pos_3d.z, radius, height);
        }
    }
}

#define MAX_ELEMENTS 50
#define MAX_MODULES 10
#define MAX_TEXT_LINES 1000
#define MAX_LINE_LENGTH 512
#define MAX_PAGE_HISTORY 20  // For back/forward navigation

#define CLIPBOARD_SIZE 10000

// Flex sizing structures
typedef enum {
    FLEX_UNIT_PIXEL,
    FLEX_UNIT_PERCENT
} FlexUnit;

typedef struct {
    float value;
    FlexUnit unit;
} FlexSize;

// UIElement structure - this must match the one in controller.c
typedef struct {
    char type[20];
    int x, y; // Original attributes from CHTML
    FlexSize width, height; // Updated to use FlexSize for percentage support
    int calculated_width, calculated_height; // Calculated values after flex resolution
    int calculated_x, calculated_y; // Calculated absolute positions after layout
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
    float alpha; // Alpha transparency
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
    // Link properties
    char href[512]; // Store href attribute for links
    // Z-level for rendering order (0-99, default 0)
    int z_level;   // Z-level for depth ordering
    // Image-specific properties
    char image_src[512];  // Store image source file path
    GLuint texture_id;    // Store texture ID for the image
    int texture_loaded;   // Flag to indicate if texture has been loaded
    int texture_loading;  // Flag to indicate if texture is currently loading (to prevent multiple attempts)
    // Video-specific properties
    char video_src[512];  // Store video source file path
    int video_loaded;     // Flag to indicate if video has been loaded
    int video_playing;    // Flag to indicate if video is playing
    int video_autoplay;   // Flag for autoplay
    int video_loop;       // Flag for loop
} UIElement;

// Comparison function for qsort to sort elements by z_level
int compare_elements_by_z_level(const void* a, const void* b) {
    UIElement* elem_a = (UIElement*)a;
    UIElement* elem_b = (UIElement*)b;
    return elem_a->z_level - elem_b->z_level;
}

// Forward declaration for canvas render function
void canvas_render_sample(int x, int y, int width, int height);
void canvas_render_camera(int x, int y, int width, int height);

// Forward declaration for the function to get shapes from the model
Shape* model_get_shapes(int* count);
char (*model_get_dir_entries(int* count))[256];

// Forward declaration for camera update function that can be called from main
int update_camera_frame(void);
void cleanup_camera(void);

// Forward declarations for audio functionality
void init_audio_system();
int play_audio(const char* filepath);
int stop_audio(const char* filepath);
int pause_audio(const char* filepath);
int resume_audio(const char* filepath);
int is_audio_playing(const char* filepath);
double get_audio_time(const char* filepath);
void cleanup_audio();

// Forward declarations for video functionality
void init_video_system();
int play_video(const char* filepath, int autoplay, int loop);
int stop_video(const char* filepath);
int pause_video(const char* filepath);
int resume_video(const char* filepath);
int is_video_playing(const char* filepath);
int update_video_texture(const char* filepath, GLuint texture_id);
int get_video_dimensions(const char* filepath, int* width, int* height);
void cleanup_video();
int are_any_videos_playing();

// Forward declarations for inline CHTML functionality
char* load_chtml_from_file(const char* filename);
int add_chtml_to_dom(const char* chtml_content);
int remove_chtml_from_dom(int start_index, int count);
int parse_inline_chtml(const char* chtml_content, int parent_element_index);
int createWindow(const char* title, const char* chtmlContent, int width, int height);
int load_chtml_file_to_window(const char* filename, int width, int height);
void update_inline_elements();

// Structure to store module paths from CHTML file
typedef struct {
    char path[512];
    int active;
    int protocol;  // 0 = pipe (default), 1 = shared memory
} ModulePath;

// Structure to hold a page's content
typedef struct {
    UIElement elements[MAX_ELEMENTS];
    int num_elements;
    ModulePath modules[MAX_MODULES];
    int num_modules;
    char filename[512];
    int loaded;
} Page;

// ContentManager to handle multiple pages and navigation
typedef struct {
    Page pages[MAX_PAGE_HISTORY];
    int current_page_index;
    int num_pages_loaded;
    char current_file_path[512];
    int navigation_active;
} ContentManager;

// Forward declarations for functions defined later in this file
int load_page(const char* filename);
int activate_page(const char* filename);
const char* get_current_page_path();
int is_page_loaded(const char* filename);
void clear_page_cache();

// Global variables - these are defined and declared in the same place
// (external declarations for other files are in respective headers)
UIElement elements[MAX_ELEMENTS];
int num_elements = 0;

// Array to store module paths from CHTML file (separate from model's modules)
ModulePath parsed_modules[MAX_MODULES];
int num_parsed_modules = 0;

// Global content manager
ContentManager content_manager;

// Function to get the current page file path
const char* get_current_page_path() {
    return content_manager.current_file_path;
}

// Function to check if a page is already loaded in cache
int is_page_loaded(const char* filename) {
    for (int i = 0; i < content_manager.num_pages_loaded; i++) {
        if (strcmp(content_manager.pages[i].filename, filename) == 0) {
            return 1;
        }
    }
    return 0;
}



// Global variables to store window dimensions
int window_width = 800;
int window_height = 600;

// Flag to indicate when display should be refreshed after DOM updates
int display_needs_refresh = 0;

// Camera-related variables
static int cam_fd = -1;
static void *cam_buffer_start = NULL;
static unsigned int cam_buffer_length;
static unsigned char *cam_rgb_data = NULL; // RGBA data for camera
static unsigned int cam_current_pixelformat = V4L2_PIX_FMT_YUYV;
static int cam_width = 640;
static int cam_height = 480;
static int cam_rgb_size; // Size of RGBA buffer (width * height * 4)
static int camera_initialized = 0;



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
        
        // Check if this is a module tag
        if (strcmp(tag_name, "module") == 0) {
            // First, find the end of the opening tag (the '>' character)
            char* open_tag_end = strchr(tag_end, '>');
            if (!open_tag_end) continue; // Malformed tag
            
            // Parse attributes from the opening tag (between tag name and >)
            int protocol = 0; // Default to pipe-based (0)
            char* attr_start = tag_end + 1; // Start after the tag name
            
            // Create a temporary null-terminated string of just the opening tag's attributes part
            char temp_char = *(open_tag_end + 1);
            *(open_tag_end + 1) = '\0';
            
            // Parse attributes by scanning the entire tag content between tag name and '>'
            char* current_pos = attr_start;
            char* tag_content_end = open_tag_end;
            
            while (current_pos < tag_content_end) {
                // Skip whitespace
                while (current_pos < tag_content_end && (*current_pos == ' ' || *current_pos == '\t')) {
                    current_pos++;
                }
                
                if (current_pos >= tag_content_end) break;
                
                // Find the '=' character for this attribute
                char* eq_sign = current_pos;
                while (eq_sign < tag_content_end && *eq_sign != '=') {
                    eq_sign++;
                }
                
                if (eq_sign >= tag_content_end) break; // No more attributes with '='
                
                // Extract attribute name
                char attr_name[50];
                int name_len = eq_sign - current_pos;
                if (name_len >= 50) name_len = 49;
                strncpy(attr_name, current_pos, name_len);
                attr_name[name_len] = '\0';
                
                // Move past '=' and find the quoted value
                eq_sign++; // Move past '='
                while (eq_sign < tag_content_end && (*eq_sign == ' ' || *eq_sign == '\t')) {
                    eq_sign++;
                }
                
                if (eq_sign >= tag_content_end || *eq_sign != '"') break; // Value must be quoted
                eq_sign++; // Move past opening quote
                
                // Find the closing quote
                char* value_end = eq_sign;
                while (value_end < tag_content_end && *value_end != '"') {
                    value_end++;
                }
                
                if (value_end >= tag_content_end) break; // No closing quote found
                
                // Extract attribute value
                char attr_value[256];
                int value_len = value_end - eq_sign;
                if (value_len >= 256) value_len = 255;
                strncpy(attr_value, eq_sign, value_len);
                attr_value[value_len] = '\0';
                
                // Check if this is the protocol attribute
                if (strcmp(attr_name, "protocol") == 0) {
                    if (strcmp(attr_value, "shared") == 0) {
                        protocol = 1;  // shared memory
                    } else if (strcmp(attr_value, "pipe") == 0) {
                        protocol = 0;  // pipe-based (default)
                    }
                }
                
                // Move to after the closing quote to find the next attribute
                current_pos = value_end + 1;
            }
            
            // Restore the original character
            *(open_tag_end + 1) = temp_char;
            
            // Now get the content between <module ...> and </module>
            char* content_start = open_tag_end + 1; // Start after '>'
            char* end_tag = strstr(content_start, "</module>");
            
            if (end_tag) {
                *end_tag = '\0'; // Temporarily terminate to get content
                // Trim whitespace from module path
                char* module_path = content_start;
                while (*module_path == ' ' || *module_path == '\t') module_path++;
                
                char* end = module_path + strlen(module_path) - 1;
                while (end > module_path && (*end == ' ' || *end == '\t')) end--;
                *(end + 1) = '\0';
                
                // Add to modules array if not full
                if (num_parsed_modules < MAX_MODULES) {
                    strncpy(parsed_modules[num_parsed_modules].path, module_path, sizeof(parsed_modules[num_parsed_modules].path) - 1);
                    parsed_modules[num_parsed_modules].path[sizeof(parsed_modules[num_parsed_modules].path) - 1] = '\0';
                    parsed_modules[num_parsed_modules].active = 1;
                    parsed_modules[num_parsed_modules].protocol = protocol;
                    num_parsed_modules++;
                    printf("Parsed module: %s (protocol: %s)\n", parsed_modules[num_parsed_modules-1].path, 
                           (parsed_modules[num_parsed_modules-1].protocol == 1) ? "shared" : "pipe");
                } else {
                    fprintf(stderr, "Warning: Maximum number of modules reached!\n");
                }
                *end_tag = '<'; // Restore the original character
            } else {
                // Simple tag without matching end tag found, look for content until end of line
                // For now, just print a debug message
                printf("Debug: Found module tag but no closing tag on this line\n");
            }
            continue; // Continue to next line, don't process as UI element
        }

        if (num_elements >= MAX_ELEMENTS) {
            fprintf(stderr, "Error: Max elements reached!\n");
            break;
        }

        elements[num_elements].parent = (stack_top > -1) ? parent_stack[stack_top] : -1;
        strcpy(elements[num_elements].type, tag_name);
        elements[num_elements].id[0] = '\0'; // Initialize ID to empty string
        elements[num_elements].alpha = 1.0f; // Default alpha to opaque
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
        elements[num_elements].href[0] = '\0'; // Initialize href to empty string
        elements[num_elements].z_level = 0;    // Initialize z-level to default (0)
        // Initialize image-specific properties
        elements[num_elements].image_src[0] = '\0';    // Initialize image source to empty string
        elements[num_elements].texture_id = 0;         // Initialize texture ID to 0
        elements[num_elements].texture_loaded = 0;     // Initialize texture loaded flag to 0
        elements[num_elements].texture_loading = 0;    // Initialize texture loading flag to 0
        
        // Initialize flex sizing attributes
        elements[num_elements].width.value = 0;
        elements[num_elements].width.unit = FLEX_UNIT_PIXEL;
        elements[num_elements].height.value = 0;
        elements[num_elements].height.unit = FLEX_UNIT_PIXEL;
        elements[num_elements].calculated_width = 0;
        elements[num_elements].calculated_height = 0;
        elements[num_elements].calculated_x = 0;
        elements[num_elements].calculated_y = 0;

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
            else if (strcmp(attr_name, "width") == 0) {
                // Check if the value contains a percentage sign
                if (strchr(attr_value, '%') != NULL) {
                    // Parse the numeric value without the % sign
                    char* percent_str = strdup(attr_value);
                    char* percent_ptr = strchr(percent_str, '%');
                    if (percent_ptr) *percent_ptr = '\0'; // Remove the % sign
                    elements[num_elements].width.value = atof(percent_str);
                    elements[num_elements].width.unit = FLEX_UNIT_PERCENT;
                    free(percent_str);
                } else {
                    // Regular pixel value
                    elements[num_elements].width.value = atof(attr_value);
                    elements[num_elements].width.unit = FLEX_UNIT_PIXEL;
                }
            }
            else if (strcmp(attr_name, "height") == 0) {
                // Check if the value contains a percentage sign
                if (strchr(attr_value, '%') != NULL) {
                    // Parse the numeric value without the % sign
                    char* percent_str = strdup(attr_value);
                    char* percent_ptr = strchr(percent_str, '%');
                    if (percent_ptr) *percent_ptr = '\0'; // Remove the % sign
                    elements[num_elements].height.value = atof(percent_str);
                    elements[num_elements].height.unit = FLEX_UNIT_PERCENT;
                    free(percent_str);
                } else {
                    // Regular pixel value
                    elements[num_elements].height.value = atof(attr_value);
                    elements[num_elements].height.unit = FLEX_UNIT_PIXEL;
                }
            }
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
            else if (strcmp(attr_name, "alpha") == 0) {
                elements[num_elements].alpha = atof(attr_value);
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
            else if (strcmp(attr_name, "href") == 0) {
                // For link elements, store the href value in the href field
                if (strcmp(elements[num_elements].type, "link") == 0 || 
                    strcmp(elements[num_elements].type, "a") == 0) {
                    strncpy(elements[num_elements].href, attr_value, 511); // Store href in href field
                    elements[num_elements].href[511] = '\0'; // Ensure null termination
                    if (strlen(elements[num_elements].label) == 0) {
                        // If there's no label, use the href as label (filename)
                        strncpy(elements[num_elements].label, attr_value, 49);
                        elements[num_elements].label[49] = '\0';
                    }
                }
            }
            else if (strcmp(attr_name, "class") == 0) {
                // Store class attribute value if needed
                // For now, we'll just use it to identify elements
                if (strcmp(elements[num_elements].type, "link") == 0 || 
                    strcmp(elements[num_elements].type, "a") == 0) {
                    // Store class in a temporary field, or use it for styling
                }
            }
            else if (strcmp(attr_name, "src") == 0) {
                // For image elements, store the source path
                if (strcmp(elements[num_elements].type, "img") == 0 || 
                    strcmp(elements[num_elements].type, "image") == 0) {
                    strncpy(elements[num_elements].image_src, attr_value, 511);
                    elements[num_elements].image_src[511] = '\0'; // Ensure null termination
                    printf("Parsed image source: %s\n", elements[num_elements].image_src);
            // Initialize the texture loading flag for this new image element
            elements[num_elements].texture_loading = 0;
                }
                // For audio elements, store the source path
                else if (strcmp(elements[num_elements].type, "audio") == 0) {
                    // For audio elements, we'll store the source in a new field
                    strncpy(elements[num_elements].image_src, attr_value, 511); // Reusing image_src field for audio path
                    elements[num_elements].image_src[511] = '\0'; // Ensure null termination
                    printf("Parsed audio source: %s\n", elements[num_elements].image_src);
                }
                // For video elements, store the source path
                else if (strcmp(elements[num_elements].type, "video") == 0) {
                    strncpy(elements[num_elements].video_src, attr_value, 511);
                    elements[num_elements].video_src[511] = '\0'; // Ensure null termination
                    printf("Parsed video source: %s\n", elements[num_elements].video_src);
                }
            }
            else if (strcmp(attr_name, "autoplay") == 0) {
                if (strcmp(elements[num_elements].type, "video") == 0) {
                    if (strcmp(attr_value, "true") == 0 || strcmp(attr_value, "1") == 0) {
                        elements[num_elements].video_autoplay = 1;
                    } else {
                        elements[num_elements].video_autoplay = 0;
                    }
                }
            }
            else if (strcmp(attr_name, "loop") == 0) {
                if (strcmp(elements[num_elements].type, "video") == 0) {
                    if (strcmp(attr_value, "true") == 0 || strcmp(attr_value, "1") == 0) {
                        elements[num_elements].video_loop = 1;
                    } else {
                        elements[num_elements].video_loop = 0;
                    }
                }
            }
            else if (strcmp(attr_name, "z_level") == 0 || strcmp(attr_name, "zlevel") == 0) {
                int level = atoi(attr_value);
                // Clamp the z-level to valid range 0-99
                if (level < 0) level = 0;
                if (level > 99) level = 99;
                elements[num_elements].z_level = level;
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
        else if (strcmp(tag_name, "img") == 0 || strcmp(tag_name, "image") == 0) {
            // Image elements can contain other elements (like text overlays), so add to stack
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "canvas") == 0) {
            // Assign a default render function to canvas elements
            // Check if this canvas has the ar attribute set to "1"
            int has_ar_attribute = 0;
            // We need to check the attributes we just parsed
            // Re-parse attributes to check for 'ar'
            char* temp_attr_start = tag_end;
            while (temp_attr_start && (temp_attr_start = strstr(temp_attr_start, " ")) != NULL) {
                temp_attr_start++; // Move past space
                char* temp_eq_sign = strchr(temp_attr_start, '=');
                if (!temp_eq_sign) break; // No more attributes

                char temp_attr_name[50];
                strncpy(temp_attr_name, temp_attr_start, temp_eq_sign - temp_attr_start);
                temp_attr_name[temp_eq_sign - temp_attr_start] = '\0';

                char* temp_value_start = temp_eq_sign + 1;
                if (*temp_value_start != '"') break; // Value not quoted
                temp_value_start++; // Move past opening quote

                char* temp_value_end = strchr(temp_value_start, '"');
                if (!temp_value_end) break; // No closing quote

                char temp_attr_value[256];
                strncpy(temp_attr_value, temp_value_start, temp_value_end - temp_value_start);
                temp_attr_value[temp_value_end - temp_value_start] = '\0';

                if (strcmp(temp_attr_name, "ar") == 0 && strcmp(temp_attr_value, "1") == 0) {
                    has_ar_attribute = 1;
                    break;
                }
                temp_attr_start = temp_value_end + 1;
            }
            
            if (has_ar_attribute) {
                elements[num_elements].canvas_render_func = canvas_render_camera;
            } else {
                elements[num_elements].canvas_render_func = canvas_render_sample;
            }
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
        else if (strcmp(tag_name, "audio") == 0) {
            // Audio elements should have play/stop buttons by default
            // Add to the stack so other elements can be contained within it if needed
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "video") == 0) {
            // Video elements - initialize video-specific properties
            elements[num_elements].video_loaded = 0;
            elements[num_elements].video_playing = 0;
            elements[num_elements].video_autoplay = 0;
            elements[num_elements].video_loop = 0;
            stack_top++;
            parent_stack[stack_top] = num_elements;
        }
        else if (strcmp(tag_name, "a") == 0) {
            // Handle anchor tags with href attribute for navigation
            // This element will be treated as a special text element that can be clicked
            strcpy(elements[num_elements].type, "link"); // Use "link" type instead of "a"
        }
        else if (strcmp(tag_name, "p") == 0) {
            // Paragraph tags can contain text and links
            // For now, treat as a container that may hold other text elements
            strcpy(elements[num_elements].type, "container"); // Use "container" type for p tags
        }

        num_elements++;
    }

    fclose(file);
}

void init_view(const char* filename) {
    init_freetype();  // Initialize FreeType for emoji rendering
    init_emoji_cache();  // Initialize emoji texture cache
    load_gl_pointers();  // Load OpenGL function pointers for texture operations
    init_texture_cache();  // Initialize texture cache
    
    // Initialize content manager if not already initialized
    if (content_manager.num_pages_loaded == 0) {
        content_manager.current_page_index = -1;
        content_manager.num_pages_loaded = 0;
        content_manager.navigation_active = 0;
        // Initialize all page structures
        for (int i = 0; i < MAX_PAGE_HISTORY; i++) {
            content_manager.pages[i].loaded = 0;
            content_manager.pages[i].num_elements = 0;
            content_manager.pages[i].num_modules = 0;
            content_manager.pages[i].filename[0] = '\0';
        }
        content_manager.current_file_path[0] = '\0';
    }
    
    // For initial loading, directly parse the content (first page doesn't need to go through cache system)
    // Initialize modules array
    for (int i = 0; i < MAX_MODULES; i++) {
        parsed_modules[i].active = 0;
        parsed_modules[i].path[0] = '\0';
    }
    num_parsed_modules = 0;
    
    // Parse the initial file directly
    parse_chtml(filename);
    
    // Also add to cache for future navigation
    // Store the current state to cache
    if (content_manager.num_pages_loaded < MAX_PAGE_HISTORY) {
        int page_index = content_manager.num_pages_loaded;
        Page* new_page = &content_manager.pages[page_index];
        
        strncpy(new_page->filename, filename, sizeof(new_page->filename) - 1);
        new_page->filename[sizeof(new_page->filename) - 1] = '\0';
        
        new_page->num_elements = num_elements;
        for (int i = 0; i < num_elements; i++) {
            memcpy(&new_page->elements[i], &elements[i], sizeof(UIElement));
        }
        
        new_page->num_modules = num_parsed_modules;
        for (int i = 0; i < num_parsed_modules; i++) {
            memcpy(&new_page->modules[i], &parsed_modules[i], sizeof(ModulePath));
        }
        
        new_page->loaded = 1;
        
        content_manager.current_page_index = page_index;
        content_manager.num_pages_loaded++;
        strncpy(content_manager.current_file_path, filename, sizeof(content_manager.current_file_path) - 1);
        content_manager.current_file_path[sizeof(content_manager.current_file_path) - 1] = '\0';
    }
}

// Function to load a page into the content manager
int load_page(const char* filename) {
    // Check if page is already loaded
    for (int i = 0; i < content_manager.num_pages_loaded; i++) {
        if (strcmp(content_manager.pages[i].filename, filename) == 0) {
            // Page already loaded, just make it current
            content_manager.current_page_index = i;
            strncpy(content_manager.current_file_path, filename, sizeof(content_manager.current_file_path) - 1);
            content_manager.current_file_path[sizeof(content_manager.current_file_path) - 1] = '\0';
            
            // Copy page content to active elements
            Page* page = &content_manager.pages[i];
            num_elements = page->num_elements;
            for (int j = 0; j < num_elements; j++) {
                memcpy(&elements[j], &page->elements[j], sizeof(UIElement));
            }
            num_parsed_modules = page->num_modules;
            for (int j = 0; j < num_parsed_modules; j++) {
                memcpy(&parsed_modules[j], &page->modules[j], sizeof(ModulePath));
            }
            return 1;
        }
    }
    
    // Check if we need to make space for a new page
    if (content_manager.num_pages_loaded >= MAX_PAGE_HISTORY) {
        // Remove the oldest page if necessary (simple rotation)
        // In a real system, we might implement a more sophisticated cache policy
        printf("Page cache full, replacing oldest page\n");
        // For now, just add to the end, overwriting the last slot
    }
    
    // Parse the new page content
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open CHTML file: %s\n", filename);
        return 0;
    }
    fclose(file);
    
    // Store the current state temporarily
    UIElement temp_elements[MAX_ELEMENTS];
    ModulePath temp_modules[MAX_MODULES];
    int temp_num_elements = num_elements;
    int temp_num_modules = num_parsed_modules;
    
    // Copy current state to temp
    for (int i = 0; i < num_elements; i++) {
        memcpy(&temp_elements[i], &elements[i], sizeof(UIElement));
    }
    for (int i = 0; i < num_parsed_modules; i++) {
        memcpy(&temp_modules[i], &parsed_modules[i], sizeof(ModulePath));
    }
    
    // Initialize modules array for the new page
    for (int i = 0; i < MAX_MODULES; i++) {
        parsed_modules[i].active = 0;
        parsed_modules[i].path[0] = '\0';
    }
    num_parsed_modules = 0;
    
    // Parse the new page
    parse_chtml(filename);
    
    // Verify parsing was successful by checking for elements
    if (num_elements == 0) {
        printf("Warning: Parsed file %s has no UI elements\n", filename);
        // Restore original state
        num_elements = temp_num_elements;
        num_parsed_modules = temp_num_modules;
        for (int i = 0; i < num_elements; i++) {
            elements[i] = temp_elements[i];
        }
        for (int i = 0; i < num_parsed_modules; i++) {
            parsed_modules[i] = temp_modules[i];
        }
        return 0;
    }
    
    // Store the new page in the content manager
    int page_index = content_manager.num_pages_loaded;
    if (page_index >= MAX_PAGE_HISTORY) {
        page_index = MAX_PAGE_HISTORY - 1; // Overwrite last slot
    } else {
        content_manager.num_pages_loaded++;
    }
    
    Page* new_page = &content_manager.pages[page_index];
    strncpy(new_page->filename, filename, sizeof(new_page->filename) - 1);
    new_page->filename[sizeof(new_page->filename) - 1] = '\0';
    
    new_page->num_elements = num_elements;
    for (int i = 0; i < num_elements; i++) {
        memcpy(&new_page->elements[i], &elements[i], sizeof(UIElement));
    }
    
    new_page->num_modules = num_parsed_modules;
    for (int i = 0; i < num_parsed_modules; i++) {
        memcpy(&new_page->modules[i], &parsed_modules[i], sizeof(ModulePath));
    }
    
    new_page->loaded = 1;
    
    // Update current page index
    content_manager.current_page_index = page_index;
    strncpy(content_manager.current_file_path, filename, sizeof(content_manager.current_file_path) - 1);
    content_manager.current_file_path[sizeof(content_manager.current_file_path) - 1] = '\0';
    
    // Restore original state (in case we're in transition mode)
    num_elements = temp_num_elements;
    num_parsed_modules = temp_num_modules;
    for (int i = 0; i < num_elements; i++) {
        memcpy(&elements[i], &temp_elements[i], sizeof(UIElement));
    }
    for (int i = 0; i < num_parsed_modules; i++) {
        memcpy(&parsed_modules[i], &temp_modules[i], sizeof(ModulePath));
    }
    
    printf("Successfully loaded page: %s (%d elements, %d modules)\n", filename, new_page->num_elements, new_page->num_modules);
    return 1;
}

// Function to activate a loaded page (copy its content to the active elements)
int activate_page(const char* filename) {
    printf("Activating page: %s\n", filename);
    
    for (int i = 0; i < content_manager.num_pages_loaded; i++) {
        if (strcmp(content_manager.pages[i].filename, filename) == 0) {
            // Verify the page has valid content before activating
            Page* page = &content_manager.pages[i];
            if (!page->loaded || page->num_elements <= 0) {
                printf("Error: Page %s exists in cache but has no valid content\n", filename);
                return 0;
            }
            
            // Copy page content to active elements
            num_elements = page->num_elements;
            for (int j = 0; j < num_elements; j++) {
                memcpy(&elements[j], &page->elements[j], sizeof(UIElement));
            }
            num_parsed_modules = page->num_modules;
            for (int j = 0; j < num_parsed_modules; j++) {
                memcpy(&parsed_modules[j], &page->modules[j], sizeof(ModulePath));
            }
            
            content_manager.current_page_index = i;
            strncpy(content_manager.current_file_path, filename, sizeof(content_manager.current_file_path) - 1);
            content_manager.current_file_path[sizeof(content_manager.current_file_path) - 1] = '\0';
            
            printf("Successfully activated page: %s\n", filename);
            return 1;
        }
    }
    
    printf("Page not found in cache: %s\n", filename);
    return 0;
}

// Function to resolve flex size values to actual pixel values
int resolve_flex_size(FlexSize flex_size, int window_size) {
    if (flex_size.unit == FLEX_UNIT_PERCENT) {
        return (int)((flex_size.value / 100.0f) * window_size);
    } else { // FLEX_UNIT_PIXEL
        return (int)flex_size.value;
    }
}

// Function to calculate layout for a single element
void calculate_element_layout(UIElement* element) {
    // Calculate width and height based on flex units
    element->calculated_width = resolve_flex_size(element->width, window_width);
    element->calculated_height = resolve_flex_size(element->height, window_height);
    
    // For now, use the same absolute positioning as before
    // The x, y values are still absolute positions from CHTML
    element->calculated_x = element->x;
    element->calculated_y = element->y;
}

// Function to calculate layout for all elements
void calculate_layout() {
    for (int i = 0; i < num_elements; i++) {
        calculate_element_layout(&elements[i]);
    }
}

void draw_element(UIElement* el) {
    int parent_x = 0;
    int parent_y = 0;

    // Accumulate all parent positions (not just immediate parent)
    int current_parent = el->parent;
    while (current_parent != -1) {
        parent_x += elements[current_parent].x;
        parent_y += elements[current_parent].y;
        current_parent = elements[current_parent].parent;
    }

    int abs_x = parent_x + el->calculated_x;
    int abs_y = calculate_absolute_y(parent_y, el->calculated_y, el->calculated_height); // Convert from real-world to OpenGL coordinates (y=0 at top becomes y=window_height at bottom)

    if (strcmp(el->type, "canvas") == 0) {
        // Draw a border for the canvas
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f); // Gray border
        glBegin(GL_LINE_LOOP);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
        glVertex2i(abs_x, abs_y + el->calculated_height);
        glEnd();
        
        // Draw canvas content without changing global OpenGL state
        if (el->canvas_render_func != NULL) {
            // Enable scissor test to clip canvas content to canvas boundaries
            glEnable(GL_SCISSOR_TEST);
            // Set scissor box to canvas boundaries in window coordinates
            // OpenGL scissor coordinates: (x, y, width, height) where (x,y) is bottom-left
            // abs_y is already the bottom Y coordinate in OpenGL's coordinate system
            glScissor(abs_x, abs_y, el->calculated_width, el->calculated_height);
            
            // Save the current modelview matrix
            glPushMatrix();
            
            // Check view mode and set up appropriate projection
            if (strcmp(el->view_mode, "3d") == 0) {
                // Switch to projection matrix to set up 3D view
                glMatrixMode(GL_PROJECTION);
                glPushMatrix(); // Save current projection matrix
                glLoadIdentity();
                
                // Set up perspective projection for 3D
                gluPerspective(el->fov, (double)el->calculated_width/(double)el->calculated_height, 0.1, 100.0);
                
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
            el->canvas_render_func(0, 0, el->calculated_width, el->calculated_height);
            
            // Restore projection matrix if we were in 3D mode
            if (strcmp(el->view_mode, "3d") == 0) {
                glMatrixMode(GL_PROJECTION);
                glPopMatrix(); // Restore previous projection matrix
                glMatrixMode(GL_MODELVIEW);
            }
            
            // Restore the modelview matrix
            glPopMatrix();
            
            // Disable scissor test after drawing canvas content
            glDisable(GL_SCISSOR_TEST);
        }
    } else if (strcmp(el->type, "menu") == 0) {
        // For context menus (like "Generic Context Menu"), only draw the menu bar when open
        // For permanent menu bars, always draw the menu bar
        int should_draw_menu_bar = 1; // By default, draw for permanent menu bars
        
        // Check if this is likely a context menu (by label or other heuristics)
        if (strstr(el->label, "context_menu") != NULL) {
            // This is a context menu - only draw if it's open
            should_draw_menu_bar = el->is_open;
        }
        
        if (should_draw_menu_bar) {
            // Draw menu bar background
            glColor4f(el->color[0], el->color[1], el->color[2], el->alpha);
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
            glVertex2i(abs_x, abs_y + el->calculated_height);
            glEnd();
            
            // Draw menu text - center it vertically in the menu bar
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            int text_offset_x = 5;
            // Center the text vertically: start from the bottom of the menu and add centered offset
            int text_offset_y = (el->calculated_height / 2) - 9;  // Use calculated_height
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
            glColor4f(el->color[0], el->color[1], el->color[2], el->alpha);
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
            glVertex2i(abs_x, abs_y + el->calculated_height);
            glEnd();
            
            // Draw menu item text
            glColor4f(0.0f, 0.0f, 0.0f, 1.0f); // Black text for menu items
            glRasterPos2i(abs_x + 5, abs_y + 5); // Adjusted to prevent text from being cut off at top
            for (char* c = el->label; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
            }
        }
        // If the parent menu IS open, menuitems are drawn in the menu section instead
    } else if (strcmp(el->type, "dirlist") == 0) {
        // Draw directory listing background
        glColor4f(0.2f, 0.2f, 0.2f, 1.0f); // Dark gray background
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
        glVertex2i(abs_x, abs_y + el->calculated_height);
        glEnd();

        // Get directory contents from the model
        int entry_count = 0;
        char (*entries)[256] = model_get_dir_entries(&entry_count);

        // Draw "dir element list:" header
        glColor4f(1.0f, 1.0f, 0.0f, 1.0f); // Yellow text for header
        glRasterPos2i(abs_x + 5, abs_y + 15);
        const char* header_text = "dir element list:";
        for (const char* c = header_text; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }

        // Draw directory entries
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White text for entries
        int line_height = 20;
        for (int i = 0; i < entry_count; i++) {
            int text_y = abs_y + 40 + (i * line_height); // Start below header
            if (text_y + line_height < abs_y + el->calculated_height) { // Make sure we don't draw outside bounds
                // TODO: Add back selection highlighting if needed
                glRasterPos2i(abs_x + 10, text_y);
                for (const char* c = entries[i]; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
                }
            }
        }
    } else if (strcmp(el->type, "textfield") == 0 || strcmp(el->type, "textarea") == 0) {
        // Background
        glColor4f(0.1f, 0.1f, 0.1f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
        glVertex2i(abs_x, abs_y + el->calculated_height);
        glEnd();
        // Border
        glColor4f(0.8f, 0.8f, 0.8f, 1.0f); // Light grey border
        glBegin(GL_LINE_LOOP);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
        glVertex2i(abs_x, abs_y + el->calculated_height);
        glEnd();
    } else if (strcmp(el->type, "audio") == 0) {
        // Draw audio player controls (play/stop buttons)
        // Draw a simple rectangular background for the audio player
        glColor4f(0.4f, 0.4f, 0.4f, 1.0f); // Dark gray background
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
        glVertex2i(abs_x, abs_y + el->calculated_height);
        glEnd();
        
        // Draw a label indicating this is an audio file
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White text
        int text_offset_x = 5;
        int text_offset_y = 15;
        glRasterPos2i(abs_x + text_offset_x, abs_y + text_offset_y);
        const char* audio_label = "Audio: ";
        for (const char* c = audio_label; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        // Draw just the filename from the path
        const char* filename_start = strrchr(el->image_src, '/');
        if (filename_start == NULL) {
            filename_start = el->image_src;
        } else {
            filename_start++; // Skip the slash
        }
        for (const char* c = filename_start; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        // Draw play/stop buttons
        int button_width = 40;
        int button_height = 20;
        int button_y = abs_y + el->calculated_height - button_height - 5;
        
        // Play button
        glColor4f(0.2f, 0.6f, 0.2f, 1.0f); // Green for play button
        glBegin(GL_QUADS);
        glVertex2i(abs_x + 10, button_y);
        glVertex2i(abs_x + 10 + button_width, button_y);
        glVertex2i(abs_x + 10 + button_width, button_y + button_height);
        glVertex2i(abs_x + 10, button_y + button_height);
        glEnd();
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White text
        glRasterPos2i(abs_x + 15, button_y + 15);
        const char* play_label = "PLAY";
        for (const char* c = play_label; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
        }
        
        // Stop button
        glColor4f(0.6f, 0.2f, 0.2f, 1.0f); // Red for stop button
        glBegin(GL_QUADS);
        glVertex2i(abs_x + 10 + button_width + 10, button_y);
        glVertex2i(abs_x + 10 + button_width + 10 + button_width, button_y);
        glVertex2i(abs_x + 10 + button_width + 10 + button_width, button_y + button_height);
        glVertex2i(abs_x + 10 + button_width + 10, button_y + button_height);
        glEnd();
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White text
        glRasterPos2i(abs_x + 15 + button_width + 10, button_y + 15);
        const char* stop_label = "STOP";
        for (const char* c = stop_label; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
        }
    } else if (strcmp(el->type, "img") == 0 || strcmp(el->type, "image") == 0) {
        // Render image element with texture
        if (el->image_src[0] != '\0') { // Check if image source is provided
            // Try to load texture if not already loaded, but do it once to avoid blocking
            if (!el->texture_loaded && !el->texture_loading && el->image_src[0] != '\0') {
                // Mark as loading to prevent multiple attempts
                el->texture_loading = 1;
                el->texture_id = load_texture_from_file(el->image_src);
                if (el->texture_id != 0) {
                    el->texture_loaded = 1;
                    printf("Loaded image texture for element: %s\n", el->image_src);
                }
                // Even if loading fails, we've tried now, so don't try again
                el->texture_loading = 0;
            }
            
            // Render image if texture is loaded
            if (el->texture_loaded && el->texture_id != 0) {
                // Save the current OpenGL state to prevent interfering with UI detection
                glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
                
                // Enable texture and blending
                glEnable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Bind the texture
                glBindTexture(GL_TEXTURE_2D, el->texture_id);
                
                // Set texture parameters for proper rendering (same as original implementation)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                
                // Draw a quad with the texture mapped onto it
                glColor4f(1.0f, 1.0f, 1.0f, el->alpha); // Use alpha from element
                glBegin(GL_QUADS);
                    // Proper texture coordinate mapping - ensure correct orientation
                    glTexCoord2f(0.0f, 1.0f); glVertex2i(abs_x, abs_y); // Top-left of texture to Top-left of quad
                    glTexCoord2f(1.0f, 1.0f); glVertex2i(abs_x + el->calculated_width, abs_y); // Top-right of texture to Top-right of quad
                    glTexCoord2f(1.0f, 0.0f); glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height); // Bottom-right of texture to Bottom-right of quad
                    glTexCoord2f(0.0f, 0.0f); glVertex2i(abs_x, abs_y + el->calculated_height); // Bottom-left of texture to Bottom-left of quad
                glEnd();
                
                // Restore the previous OpenGL state
                glPopAttrib();
            } else {
                // Fallback: draw a placeholder rectangle for image if texture not loaded
                glColor4f(0.8f, 0.8f, 0.8f, 0.5f); // Light gray with transparency as placeholder
                glBegin(GL_QUADS);
                glVertex2i(abs_x, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
                glVertex2i(abs_x, abs_y + el->calculated_height);
                glEnd();
                
                // Draw a border around the placeholder
                glColor4f(0.5f, 0.5f, 0.5f, 1.0f); // Dark gray border
                glBegin(GL_LINE_LOOP);
                glVertex2i(abs_x, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
                glVertex2i(abs_x, abs_y + el->calculated_height);
                glEnd();
                
                // Draw image path as text if available (truncated if needed)
                if (strlen(el->image_src) > 0) {
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White text
                    char display_path[64];
                    if (strlen(el->image_src) > 60) {
                        strncpy(display_path, el->image_src, 57);
                        strcpy(display_path + 57, "...");
                    } else {
                        strcpy(display_path, el->image_src);
                    }
                    
                    // Center text vertically and horizontally in the image area
                    int text_width = 0;
                    for (char* c = display_path; *c != '\0'; c++) {
                        text_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, *c);
                    }
                    
                    int text_x = abs_x + (el->calculated_width - text_width) / 2;
                    int text_y = abs_y + el->calculated_height / 2;
                    
                    // Adjust y position to align text better vertically
                    glRasterPos2i(text_x, text_y);
                    for (char* c = display_path; *c != '\0'; c++) {
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                    }
                }
            }
        } else {
            // If no image source provided, draw a placeholder rectangle
            glColor4f(0.9f, 0.9f, 0.9f, 0.5f); // Light gray with transparency as placeholder
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
            glVertex2i(abs_x, abs_y + el->calculated_height);
            glEnd();
        }
    } else if (strcmp(el->type, "video") == 0) {
        // Render video element with texture
        if (el->video_src[0] != '\0') { // Check if video source is provided
            // Initialize video if not already loaded
            if (!el->video_loaded && el->video_src[0] != '\0') {
                // Initialize video texture - load a placeholder texture first
                if (el->texture_id == 0) {
                    glGenTextures(1, (GLuint*)&el->texture_id);
                }
                
                // Try to load the video file if not already loaded
                // This is a simplified implementation - in a real app, you'd manage video loading properly
                el->video_loaded = 1;
                printf("Loaded video element: %s\n", el->video_src);
                
                // Start video playback if autoplay is set
                if (el->video_autoplay && !el->video_playing) {
                    // In a real implementation, we'd call play_video here
                    el->video_playing = 1;
                    printf("Starting autoplay for video: %s\n", el->video_src);
                }
            }
            
            // Update video texture with current frame
            if (el->video_loaded && el->texture_id != 0) {
                // Try to update the video texture with the current frame from the video
                if (strlen(el->video_src) > 0) {
                    int result = update_video_texture(el->video_src, el->texture_id);
                    if (result == 0) {
                        // If update_video_texture failed, fall back to placeholder
                        // Generate a simple pattern to simulate video texture
                        static int frame_counter = 0;
                        frame_counter++;
                        
                        // Generate a simple pattern to simulate video texture
                        int width = el->calculated_width;
                        int height = el->calculated_height;
                        unsigned char *temp_texture = malloc(width * height * 3);
                        
                        for (int y = 0; y < height; y++) {
                            for (int x = 0; x < width; x++) {
                                int idx = (y * width + x) * 3;
                                // Create a moving pattern to simulate video
                                temp_texture[idx] = (x + frame_counter) % 256;     // R
                                temp_texture[idx + 1] = (y + frame_counter) % 256; // G
                                temp_texture[idx + 2] = (x + y + frame_counter) % 256; // B
                            }
                        }
                        
                        // Bind and update the texture
                        glBindTexture(GL_TEXTURE_2D, el->texture_id);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, temp_texture);
                        
                        free(temp_texture);
                    }
                }
                
                // Save the current OpenGL state to prevent interfering with UI detection
                glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);
                
                // Enable texture and blending
                glEnable(GL_TEXTURE_2D);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
                // Bind the texture
                glBindTexture(GL_TEXTURE_2D, el->texture_id);
                
                // Set texture parameters for proper rendering
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                
                // Draw a quad with the texture mapped onto it, preserving aspect ratio
                glColor4f(1.0f, 1.0f, 1.0f, el->alpha); // Use alpha from element

                // Get original video dimensions to calculate proper aspect ratio
                int video_width = 0, video_height = 0;
                if (get_video_dimensions(el->video_src, &video_width, &video_height) == 0 && video_width > 0 && video_height > 0) {
                    // Calculate aspect ratio preserving dimensions
                    float video_aspect = (float)video_width / (float)video_height;
                    float display_aspect = (float)el->calculated_width / (float)el->calculated_height;
                    
                    int render_width, render_height;
                    int render_x = abs_x;
                    int render_y = abs_y;
                    
                    if (video_aspect > display_aspect) {
                        // Video is wider than display area - fit to width
                        render_width = el->calculated_width;
                        render_height = (int)(el->calculated_width / video_aspect);
                        render_y = abs_y + (el->calculated_height - render_height) / 2; // Center vertically
                    } else {
                        // Video is taller than display area - fit to height  
                        render_height = el->calculated_height;
                        render_width = (int)(el->calculated_height * video_aspect);
                        render_x = abs_x + (el->calculated_width - render_width) / 2; // Center horizontally
                    }
                    
                    glBegin(GL_QUADS);
                    // Map texture to properly sized quad to maintain aspect ratio
                    glTexCoord2f(0.0f, 1.0f); glVertex2i(render_x, render_y); // Top-left of texture to Top-left of quad
                    glTexCoord2f(1.0f, 1.0f); glVertex2i(render_x + render_width, render_y); // Top-right of texture to Top-right of quad
                    glTexCoord2f(1.0f, 0.0f); glVertex2i(render_x + render_width, render_y + render_height); // Bottom-right of texture to Bottom-right of quad
                    glTexCoord2f(0.0f, 0.0f); glVertex2i(render_x, render_y + render_height); // Bottom-left of texture to Bottom-left of quad
                    glEnd();
                } else {
                    // Fallback to original size if dimensions not available
                    glBegin(GL_QUADS);
                    // Proper texture coordinate mapping for video
                    // Video texture coordinates (0,0) is top-left, (1,1) is bottom-right in video data
                    // But OpenGL texture coordinates (0,0) is bottom-left when displayed
                    glTexCoord2f(0.0f, 1.0f); glVertex2i(abs_x, abs_y); // Top-left of texture to Top-left of quad
                    glTexCoord2f(1.0f, 1.0f); glVertex2i(abs_x + el->calculated_width, abs_y); // Top-right of texture to Top-right of quad
                    glTexCoord2f(1.0f, 0.0f); glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height); // Bottom-right of texture to Bottom-right of quad
                    glTexCoord2f(0.0f, 0.0f); glVertex2i(abs_x, abs_y + el->calculated_height); // Bottom-left of texture to Bottom-left of quad
                    glEnd();
                }
                
                // Restore the previous OpenGL state
                glPopAttrib();
            } else {
                // Fallback: draw a placeholder rectangle for video if not loaded
                glColor4f(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray background
                glBegin(GL_QUADS);
                glVertex2i(abs_x, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
                glVertex2i(abs_x, abs_y + el->calculated_height);
                glEnd();
                
                // Draw a border around the placeholder
                glColor4f(0.5f, 0.5f, 0.5f, 1.0f); // Gray border
                glBegin(GL_LINE_LOOP);
                glVertex2i(abs_x, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y);
                glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
                glVertex2i(abs_x, abs_y + el->calculated_height);
                glEnd();
                
                // Draw video path as text if available
                if (strlen(el->video_src) > 0) {
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White text
                    char display_path[64];
                    if (strlen(el->video_src) > 60) {
                        strncpy(display_path, el->video_src, 57);
                        strcpy(display_path + 57, "...");
                    } else {
                        strcpy(display_path, el->video_src);
                    }
                    
                    // Center text vertically and horizontally in the video area
                    int text_width = 0;
                    for (char* c = display_path; *c != '\0'; c++) {
                        text_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, *c);
                    }
                    
                    int text_x = abs_x + (el->calculated_width - text_width) / 2;
                    int text_y = abs_y + el->calculated_height / 2;
                    
                    // Adjust y position to align text better vertically
                    glRasterPos2i(text_x, text_y);
                    for (char* c = display_path; *c != '\0'; c++) {
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                    }
                }
                
                // Draw "VIDEO" label
                glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // Red text for label
                int label_x = abs_x + (el->calculated_width - glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, 'V')*5) / 2;
                int label_y = abs_y + el->calculated_height / 2 + 20;
                glRasterPos2i(label_x, label_y);
                const char* video_label = "VIDEO";
                for (const char* c = video_label; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
                }
            }
        } else {
            // If no video source provided, draw a placeholder rectangle
            glColor4f(0.2f, 0.2f, 0.2f, 0.5f); // Dark gray with transparency as placeholder
            glBegin(GL_QUADS);
            glVertex2i(abs_x, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y);
            glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
            glVertex2i(abs_x, abs_y + el->calculated_height);
            glEnd();
        }
    } else if (strcmp(el->type, "header") == 0 || strcmp(el->type, "button") == 0 || strcmp(el->type, "panel") == 0 || strcmp(el->type, "checkbox") == 0 || strcmp(el->type, "slider") == 0) {
        if (strcmp(el->type, "checkbox") == 0) {
            glColor4f(0.8f, 0.8f, 0.8f, 1.0f); // Light background for checkbox square
        } else if (strcmp(el->type, "slider") == 0) {
            glColor4f(0.3f, 0.3f, 0.3f, 1.0f); // Dark background for slider track
        } else {
            glColor4f(el->color[0], el->color[1], el->color[2], el->alpha);
        }
        glBegin(GL_QUADS);
        glVertex2i(abs_x, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y);
        glVertex2i(abs_x + el->calculated_width, abs_y + el->calculated_height);
        glVertex2i(abs_x, abs_y + el->calculated_height);
        glEnd();

        if (strcmp(el->type, "checkbox") == 0 && el->is_checked) {
            // Draw a simple checkmark (upright V shape, adjusted for inverted Y)
            glColor4f(0.0f, 0.0f, 0.0f, 1.0f); // Black checkmark
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            // First segment: bottom-left to top-middle
            glVertex2i(abs_x + el->calculated_width * 0.2, abs_y + el->calculated_height * 0.8); // Visually lower
            glVertex2i(abs_x + el->calculated_width * 0.4, abs_y + el->calculated_height * 0.2); // Visually higher
            // Second segment: top-middle to bottom-right
            glVertex2i(abs_x + el->calculated_width * 0.4, abs_y + el->calculated_height * 0.2); // Visually higher
            glVertex2i(abs_x + el->calculated_width * 0.8, abs_y + el->calculated_height * 0.8); // Visually lower
            glEnd();
            glLineWidth(1.0f);
        } else if (strcmp(el->type, "slider") == 0) {
            // Check if this is a vertical slider by looking at the aspect ratio
            int is_vertical = (el->calculated_height > el->calculated_width);  // Use calculated dimensions
            
            if (is_vertical) {
                // Draw vertical slider track (background)
                glColor4f(0.3f, 0.3f, 0.3f, 1.0f); // Dark background for slider track
                glBegin(GL_QUADS);
                glVertex2i(abs_x + el->calculated_width/2 - 3, abs_y); // Centered track
                glVertex2i(abs_x + el->calculated_width/2 + 3, abs_y);
                glVertex2i(abs_x + el->calculated_width/2 + 3, abs_y + el->calculated_height);
                glVertex2i(abs_x + el->calculated_width/2 - 3, abs_y + el->calculated_height);
                glEnd();
            
                // Draw vertical slider thumb
                float normalized_pos = (float)(el->slider_value - el->slider_min) / (el->slider_max - el->slider_min);
                float thumb_pos_y = abs_y + normalized_pos * (el->calculated_height - 10); // Use calculated_height, position based on normalized value
                glColor4f(0.8f, 0.8f, 0.8f, 1.0f); // Light grey for thumb
                glBegin(GL_QUADS);
                glVertex2i(abs_x, thumb_pos_y); // Thumb wider than track
                glVertex2i(abs_x + el->calculated_width, thumb_pos_y);
                glVertex2i(abs_x + el->calculated_width, thumb_pos_y + 10); // Thumb height of 10
                glVertex2i(abs_x, thumb_pos_y + 10);
                glEnd();

                // Draw slider value as text
                char value_str[10];
                sprintf(value_str, "%d", el->slider_value);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glRasterPos2i(abs_x + el->calculated_width + 5, abs_y + el->calculated_height / 2 - 5);
                for (char* c = value_str; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                }
            } else {
                // Original horizontal slider implementation
                float thumb_pos_x = abs_x + (float)(el->slider_value - el->slider_min) / (el->slider_max - el->slider_min) * (el->calculated_width - 10); // Use calculated_width, 10 is thumb width
                glColor4f(0.8f, 0.8f, 0.8f, 1.0f); // Light grey for thumb
                glBegin(GL_QUADS);
                glVertex2i(thumb_pos_x, abs_y);
                glVertex2i(thumb_pos_x + 10, abs_y);
                glVertex2i(thumb_pos_x + 10, abs_y + el->calculated_height);
                glVertex2i(thumb_pos_x, abs_y + el->calculated_height);
                glEnd();

                // Draw slider value as text
                char value_str[10];
                sprintf(value_str, "%d", el->slider_value);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glRasterPos2i(abs_x + el->calculated_width + 5, abs_y + el->calculated_height / 2 - 5);
                for (char* c = value_str; *c != '\0'; c++) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                }
            }
        }
    }

    if (strcmp(el->type, "text") == 0 || strcmp(el->type, "button") == 0 || strcmp(el->type, "checkbox") == 0 || strcmp(el->type, "link") == 0) {
        // Check if the text contains emojis
        if (contains_emoji(el->label)) {
            // Use emoji-aware text rendering
            int text_offset_x = 5;
            int text_offset_y = 15;
            if (strcmp(el->type, "checkbox") == 0) {
                text_offset_x = el->calculated_width + 5; // Label next to checkbox
                text_offset_y = el->calculated_height / 2 - 5; // Center label vertically
            }
            
            float text_color[3] = {1.0f, 1.0f, 1.0f}; // Default white
            if (strcmp(el->type, "link") == 0) {
                text_color[0] = 0.0f; // Red component
                text_color[1] = 0.0f; // Green component  
                text_color[2] = 1.0f; // Blue component (blue links)
            }
            float text_pos_x = abs_x + text_offset_x;
            float text_pos_y = abs_y + text_offset_y + 15; // Use slightly adjusted Y to align with text baseline
            
            render_text_with_emojis(el->label, text_pos_x, text_pos_y, text_color); // Use original approach
        } else {
            // Use original GLUT rendering for non-emoji text
            int text_offset_x = 5;
            int text_offset_y = 15;
            if (strcmp(el->type, "checkbox") == 0) {
                text_offset_x = el->calculated_width + 5; // Label next to checkbox
                text_offset_y = el->calculated_height / 2 - 5; // Center label vertically
            }
            
            if (strcmp(el->type, "link") == 0) {
                glColor4f(0.0f, 0.0f, 1.0f, 1.0f); // Blue color for links
            } else {
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // White for other elements
            }
            
            glRasterPos2i(abs_x + text_offset_x, abs_y + text_offset_y);
            for (char* c = el->label; *c != '\0'; c++) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
            }
            
            // Draw underline for links
            if (strcmp(el->type, "link") == 0) {
                int text_width = 0;
                for (char* c = el->label; *c != '\0'; c++) {
                    text_width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);
                }
                
                // Draw underline under the text
                glColor4f(0.0f, 0.0f, 1.0f, 1.0f); // Blue underline
                glBegin(GL_LINES);
                glVertex2i(abs_x + text_offset_x, abs_y + text_offset_y - 2);
                glVertex2i(abs_x + text_offset_x + text_width, abs_y + text_offset_y - 2);
                glEnd();
            }
        }
    } else if (strcmp(el->type, "textfield") == 0 || strcmp(el->type, "textarea") == 0) {
        // Draw multi-line text, with emoji support when needed
        int line_height = 20; // Approximate height for each line
        int start_y = abs_y + 15; // Starting y position for first line
        
        for (int line = 0; line < el->num_lines; line++) {
            if (start_y + (line * line_height) >= abs_y + el->calculated_height) break; // Don't draw outside bounds
            
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
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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

// Camera-related helper functions
static int xioctl_cam(int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

static void errno_exit_cam(const char *s) {
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
}

static void yuyv_to_rgb_cam(unsigned char *yuyv, unsigned char *rgb, int size) {
    int i, j;
    int max_pixels = (cam_rgb_size / 4); // RGBA (4 bytes per pixel)
    for (i = 0, j = 0; i < size - 3 && j < cam_rgb_size - 7; i += 4, j += 8) {
        int y0 = yuyv[i + 0];
        int u  = yuyv[i + 1] - 128;
        int y1 = yuyv[i + 2];
        int v  = yuyv[i + 3] - 128;

        // First pixel
        int r = (298 * y0 + 409 * v + 128) >> 8;
        int g = (298 * y0 - 100 * u - 208 * v + 128) >> 8;
        int b = (298 * y0 + 516 * u + 128) >> 8;
        rgb[j + 0] = (r < 0) ? 0 : (r > 255) ? 255 : r;
        rgb[j + 1] = (g < 0) ? 0 : (g > 255) ? 255 : g;
        rgb[j + 2] = (b < 0) ? 0 : (b > 255) ? 255 : b;
        rgb[j + 3] = 255; // Alpha = 255 (opaque)

        // Second pixel
        r = (298 * y1 + 409 * v + 128) >> 8;
        g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;
        b = (298 * y1 + 516 * u + 128) >> 8;
        rgb[j + 4] = (r < 0) ? 0 : (r > 255) ? 255 : r;
        rgb[j + 5] = (g < 0) ? 0 : (g > 255) ? 255 : g;
        rgb[j + 6] = (b < 0) ? 0 : (b > 255) ? 255 : b;
        rgb[j + 7] = 255; // Alpha = 255 (opaque)
    }
}

static void yuv420_to_rgb_cam(unsigned char *yuv, unsigned char *rgb, int width, int height) {
    int y_size = width * height;
    int uv_size = y_size / 4;
    unsigned char *y = yuv;
    unsigned char *u = yuv + y_size;
    unsigned char *v = u + uv_size;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int y_idx = j * width + i;
            int uv_idx = (j / 2) * (width / 2) + (i / 2);
            int y_val = y[y_idx];
            int u_val = u[uv_idx] - 128;
            int v_val = v[uv_idx] - 128;

            int r = (298 * y_val + 409 * v_val + 128) >> 8;
            int g = (298 * y_val - 100 * u_val - 208 * v_val + 128) >> 8;
            int b = (298 * y_val + 516 * u_val + 128) >> 8;

            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);

            int rgb_idx = (j * width + i) * 4; // RGBA
            rgb[rgb_idx + 0] = r;
            rgb[rgb_idx + 1] = g;
            rgb[rgb_idx + 2] = b;
            rgb[rgb_idx + 3] = 255; // Alpha = 255 (opaque)
        }
    }
}

// We'll add a placeholder for MJPG handling, but for now just log that it's not supported
static void mjpeg_to_rgb_cam(unsigned char *mjpeg_data, int size, unsigned char *rgb) {
    // For now, just fill with a pattern to indicate MJPEG data received
    // In a real implementation, you'd use libjpeg or stb_image to decode
    for (int i = 0; i < cam_rgb_size; i += 4) {
        // Create a simple pattern to show MJPEG was received
        rgb[i] = (i / 4) % 256;       // R
        rgb[i+1] = ((i / 4) / 10) % 256; // G  
        rgb[i+2] = ((i / 4) * 2) % 256;  // B
        rgb[i+3] = 255;               // A
    }
    printf("MJPEG data received but not properly decoded - using placeholder pattern\n");
}

static int try_camera_format(int width, int height, unsigned int pixelformat) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (-1 == xioctl_cam(cam_fd, VIDIOC_S_FMT, &fmt)) {
        fprintf(stderr, "Failed to set format %c%c%c%c at %dx%d\n", 
                pixelformat & 0xFF, (pixelformat >> 8) & 0xFF, 
                (pixelformat >> 16) & 0xFF, (pixelformat >> 24) & 0xFF,
                width, height);
        return 0;
    }

    if (-1 == xioctl_cam(cam_fd, VIDIOC_G_FMT, &fmt)) {
        fprintf(stderr, "VIDIOC_G_FMT error\n");
        return 0;
    }

    // Update camera width, height, and rgb_size if device adjusts resolution
    if (fmt.fmt.pix.width != width || fmt.fmt.pix.height != height) {
        fprintf(stderr, "Format adjusted to %ux%u\n", 
                fmt.fmt.pix.width, fmt.fmt.pix.height);
        cam_width = fmt.fmt.pix.width;
        cam_height = fmt.fmt.pix.height;
        cam_rgb_size = cam_width * cam_height * 4; // RGBA
        // Reallocate cam_rgb_data
        unsigned char *new_cam_rgb_data = realloc(cam_rgb_data, cam_rgb_size);
        if (!new_cam_rgb_data) {
            fprintf(stderr, "Failed to reallocate cam_rgb_data\n");
            return 0;
        }
        cam_rgb_data = new_cam_rgb_data;
        memset(cam_rgb_data, 0, cam_rgb_size); // Clear buffer to prevent residual data
    }

    printf("Using camera format: %c%c%c%c, size: %ux%u, bytes: %u\n", 
           fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF, 
           (fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF,
           fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.sizeimage);
    cam_buffer_length = fmt.fmt.pix.sizeimage;
    cam_current_pixelformat = fmt.fmt.pix.pixelformat;
    return 1;
}

static void init_camera_mmap(void) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl_cam(cam_fd, VIDIOC_REQBUFS, &req)) {
        errno_exit_cam("VIDIOC_REQBUFS");
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == xioctl_cam(cam_fd, VIDIOC_QUERYBUF, &buf)) {
        errno_exit_cam("VIDIOC_QUERYBUF");
    }

    cam_buffer_length = buf.length;
    cam_buffer_start = mmap(NULL, cam_buffer_length, PROT_READ | PROT_WRITE, MAP_SHARED, cam_fd, buf.m.offset);
    if (MAP_FAILED == cam_buffer_start) errno_exit_cam("mmap");
}

static void init_camera_device(void) {
    struct v4l2_capability cap;
    if (-1 == xioctl_cam(cam_fd, VIDIOC_QUERYCAP, &cap)) {
        errno_exit_cam("VIDIOC_QUERYCAP");
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "/dev/video0 is no video capture device\n");
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "/dev/video0 does not support streaming i/o\n");
        exit(EXIT_FAILURE);
    }

    // Try the format that works on this machine: V4L2_PIX_FMT_YUYV
    if (try_camera_format(640, 480, V4L2_PIX_FMT_YUYV)) {
        init_camera_mmap();
        return;
    }
    if (try_camera_format(320, 240, V4L2_PIX_FMT_YUYV)) {
        init_camera_mmap();
        return;
    }
    if (try_camera_format(1280, 720, V4L2_PIX_FMT_YUYV)) {
        init_camera_mmap();
        return;
    }
    // Other formats as fallback
    if (try_camera_format(640, 480, V4L2_PIX_FMT_YUV420)) {
        init_camera_mmap();
        return;
    }
    if (try_camera_format(320, 240, V4L2_PIX_FMT_YUV420)) {
        init_camera_mmap();
        return;
    }
    if (try_camera_format(1280, 720, V4L2_PIX_FMT_YUV420)) {
        init_camera_mmap();
        return;
    }
    if (try_camera_format(1280, 720, V4L2_PIX_FMT_MJPEG)) {
        init_camera_mmap();
        return;
    }
    if (try_camera_format(320, 240, V4L2_PIX_FMT_MJPEG)) {
        init_camera_mmap();
        return;
    }

    fprintf(stderr, "No supported format/resolution found for /dev/video0\n");
    exit(EXIT_FAILURE);
}

static void start_camera_capturing(void) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == xioctl_cam(cam_fd, VIDIOC_QBUF, &buf)) {
        errno_exit_cam("VIDIOC_QBUF");
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl_cam(cam_fd, VIDIOC_STREAMON, &type)) {
        errno_exit_cam("VIDIOC_STREAMON");
    }
}

static int read_camera_frame(void) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == xioctl_cam(cam_fd, VIDIOC_DQBUF, &buf)) {
        if (errno == EAGAIN) return 0;
        errno_exit_cam("VIDIOC_DQBUF");
    }

    // Clear cam_rgb_data before processing to ensure no residual data
    memset(cam_rgb_data, 0, cam_rgb_size);

    // Convert frame to RGBA
    if (cam_current_pixelformat == V4L2_PIX_FMT_YUYV) {
        yuyv_to_rgb_cam(cam_buffer_start, cam_rgb_data, buf.bytesused);
    } else if (cam_current_pixelformat == V4L2_PIX_FMT_YUV420) {
        yuv420_to_rgb_cam(cam_buffer_start, cam_rgb_data, cam_width, cam_height);
    } else if (cam_current_pixelformat == V4L2_PIX_FMT_MJPEG) {
        mjpeg_to_rgb_cam(cam_buffer_start, buf.bytesused, cam_rgb_data);
    } else {
        fprintf(stderr, "Unsupported camera pixel format: %c%c%c%c\n", 
                cam_current_pixelformat & 0xFF, (cam_current_pixelformat >> 8) & 0xFF, 
                (cam_current_pixelformat >> 16) & 0xFF, (cam_current_pixelformat >> 24) & 0xFF);
        return 0;
    }

    if (-1 == xioctl_cam(cam_fd, VIDIOC_QBUF, &buf)) {
        errno_exit_cam("VIDIOC_QBUF");
    }

    return 1;
}

static void init_camera(void) {
    if (camera_initialized) {
        return; // Already initialized
    }
    
    // Initialize camera RGB_SIZE (RGBA)
    cam_rgb_size = cam_width * cam_height * 4;

    cam_fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
    if (-1 == cam_fd) {
        fprintf(stderr, "Cannot open '/dev/video0': %d, %s\n", errno, strerror(errno));
        return;
    }

    init_camera_device();
    start_camera_capturing();
    
    // Allocate the RGB buffer
    cam_rgb_data = malloc(cam_rgb_size);
    if (!cam_rgb_data) {
        fprintf(stderr, "Failed to allocate cam_rgb_data\n");
        close(cam_fd);
        cam_fd = -1;
        return;
    }
    memset(cam_rgb_data, 0, cam_rgb_size); // Initialize to zero
    
    camera_initialized = 1;
    printf("Camera initialized successfully\n");
}

// Function to cleanup camera resources
void cleanup_camera(void) {
    if (cam_fd != -1) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl_cam(cam_fd, VIDIOC_STREAMOFF, &type);
        if (cam_buffer_start) {
            munmap(cam_buffer_start, cam_buffer_length);
            cam_buffer_start = NULL;
        }
        close(cam_fd);
        cam_fd = -1;
    }
    if (cam_rgb_data) {
        free(cam_rgb_data);
        cam_rgb_data = NULL;
    }
    camera_initialized = 0;
}

// Function to update camera frame - call this periodically from idle function
int update_camera_frame(void) {
    if (!camera_initialized) {
        return 0;
    }
    
    fd_set fds;
    struct timeval tv = {0, 10000}; // 10ms timeout
    FD_ZERO(&fds);
    FD_SET(cam_fd, &fds);

    int r = select(cam_fd + 1, &fds, NULL, NULL, &tv);
    if (-1 == r && EINTR != errno) {
        fprintf(stderr, "camera select error %d, %s\n", errno, strerror(errno));
        return 0;
    }

    if (FD_ISSET(cam_fd, &fds)) {
        return read_camera_frame();
    }
    return 0;
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Calculate layout for flex sizing before drawing
    calculate_layout();

    // Sort elements by z_level to ensure proper depth ordering
    // Only sort if there are elements to avoid unnecessary work
    if (num_elements > 1) {
        qsort(elements, num_elements, sizeof(UIElement), compare_elements_by_z_level);
    }

    // Draw all regular elements first (now sorted by z_level)
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
    // Note: These are not affected by z_level sorting as they always appear on top
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "menu") == 0 && elements[i].is_open) {
            // Draw this open menu's submenu items with proper positioning
            // Find and draw all menuitems that belong to this menu
            int parent_menu = i;
            int parent_menu_abs_x = 0;
            int parent_menu_abs_y = 0;
            
            // Validate parent index to prevent segmentation fault
            int parent_menu_parent = elements[parent_menu].parent;
            while (parent_menu_parent != -1 && parent_menu_parent < num_elements) {
                parent_menu_abs_x += elements[parent_menu_parent].calculated_x;
                parent_menu_abs_y += elements[parent_menu_parent].calculated_y;
                parent_menu_parent = elements[parent_menu_parent].parent;
            }
            
            int submenu_x = parent_menu_abs_x + elements[parent_menu].calculated_x;
            // Calculate submenu position using the same conversion as in draw_element but positioning below the parent menu
            int parent_menu_abs_y_coord = parent_menu_abs_y + elements[parent_menu].calculated_y;
            int parent_menu_bottom_opengl = window_height - (parent_menu_abs_y_coord + elements[parent_menu].calculated_height);
            int submenu_top_opengl = parent_menu_bottom_opengl - (elements[parent_menu].menu_items_count * 20);
            
            // Draw submenu background (only if menu_items_count is valid)
            if (elements[parent_menu].menu_items_count > 0) {
                glColor4f(elements[parent_menu].color[0], elements[parent_menu].color[1], elements[parent_menu].color[2], elements[parent_menu].alpha * 0.9f); // Slightly transparent background
                glBegin(GL_QUADS);
                glVertex2i(submenu_x, submenu_top_opengl);
                glVertex2i(submenu_x + elements[parent_menu].calculated_width, submenu_top_opengl);
                glVertex2i(submenu_x + elements[parent_menu].calculated_width, parent_menu_bottom_opengl);
                glVertex2i(submenu_x, parent_menu_bottom_opengl);
                glEnd();
                
                // Draw border around submenu
                glColor4f(0.7f, 0.7f, 0.7f, 1.0f); // Gray border
                glBegin(GL_LINE_LOOP);
                glVertex2i(submenu_x, submenu_top_opengl);
                glVertex2i(submenu_x + elements[parent_menu].calculated_width, submenu_top_opengl);
                glVertex2i(submenu_x + elements[parent_menu].calculated_width, parent_menu_bottom_opengl);
                glVertex2i(submenu_x, parent_menu_bottom_opengl);
                glEnd();
            }
            
            // Draw each submenu item
            int item_position = 0;
            for (int j = 0; j < num_elements; j++) {
                if (strcmp(elements[j].type, "menuitem") == 0 && 
                    elements[j].parent == i) {
                    // Calculate absolute position for this submenu item in OpenGL coordinates
                    int abs_x = submenu_x;
                    int abs_y = submenu_top_opengl + (item_position * 20);
                    
                    // Draw menu item background
                    glColor4f(elements[j].color[0], elements[j].color[1], elements[j].color[2], elements[j].alpha);
                    glBegin(GL_QUADS);
                    glVertex2i(abs_x, abs_y);
                    glVertex2i(abs_x + elements[j].calculated_width, abs_y);
                    glVertex2i(abs_x + elements[j].calculated_width, abs_y + 20); // Fixed height of 20 for submenu items
                    glVertex2i(abs_x, abs_y + 20);
                    glEnd();
                    
                    // Draw menu item text
                    glColor4f(0.0f, 0.0f, 0.0f, 1.0f); // Black text for menu items
                    glRasterPos2i(abs_x + 5, abs_y + 5); // Lowered text position to prevent top half from being cut off
                    for (char* c = elements[j].label; *c != '\0'; c++) {
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
                    }
                    
                    item_position++;
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
        if (strcmp(elements[i].type, "canvas") == 0 && elements[i].calculated_width == width && elements[i].calculated_height == height) {
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

        // Render shapes in 3D space
        for (int i = 0; i < shape_count; i++) {
            Shape* s = &shapes[i];
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_3d_shape(s, width, height);
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
        glColor4f(0.8f, 0.9f, 1.0f, current_canvas->alpha); // Light blue background
        glBegin(GL_QUADS);
        glVertex2i(0, 0);  // The drawing is translated to the canvas's origin
        glVertex2i(width, 0);
        glVertex2i(width, height);
        glVertex2i(0, height);
        glEnd();

        // Render each shape in 2D
        for (int i = 0; i < shape_count; i++) {
            Shape* s = &shapes[i];
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_2d_shape(s);
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

// Getter functions for modules
ModulePath* get_modules(int* count) {
    *count = num_parsed_modules;
    return parsed_modules;
}

// Function to clean up the view state for reloading
// Function to get window properties
void get_window_properties(int* x, int* y, int* width, int* height, char* title, int title_size) {
    *x = 100;      // Default values
    *y = 100;
    *width = 800;
    *height = 600;
    strncpy(title, "C-HTML Prototype", title_size - 1);
    title[title_size - 1] = '\0';
    
    // Find window element and get its properties
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "window") == 0) {
            if (elements[i].x != 0) *x = elements[i].x;
            if (elements[i].y != 0) *y = elements[i].y;
            if (elements[i].calculated_width != 0) *width = elements[i].calculated_width;
            if (elements[i].calculated_height != 0) *height = elements[i].calculated_height;
            if (strlen(elements[i].label) > 0) {
                strncpy(title, elements[i].label, title_size - 1);
                title[title_size - 1] = '\0';
            }
            break;
        }
    }
}

void cleanup_view() {
    // Clean up any loaded textures to prevent memory leaks
    for (int i = 0; i < num_elements; i++) {
        if ((strcmp(elements[i].type, "img") == 0 || strcmp(elements[i].type, "image") == 0) && 
            elements[i].texture_loaded && elements[i].texture_id != 0) {
            glDeleteTextures(1, (GLuint*)&elements[i].texture_id);
            elements[i].texture_id = 0;
            elements[i].texture_loaded = 0;
            printf("Cleaned up texture for image element: %s\n", elements[i].image_src);
        }
        // Also clean up video textures
        else if (strcmp(elements[i].type, "video") == 0 && 
                 elements[i].video_loaded && elements[i].texture_id != 0) {
            glDeleteTextures(1, (GLuint*)&elements[i].texture_id);
            elements[i].texture_id = 0;
            printf("Cleaned up texture for video element: %s\n", elements[i].video_src);
        }
    }
    
    // Reset elements array
    for (int i = 0; i < MAX_ELEMENTS; i++) {
        memset(&elements[i], 0, sizeof(UIElement));
    }
    num_elements = 0;
    
    // Reset module paths
    for (int i = 0; i < MAX_MODULES; i++) {
        memset(&parsed_modules[i], 0, sizeof(ModulePath));
    }
    num_parsed_modules = 0;
    
    // Reset other state variables as needed
    // For example, reset text content arrays, etc.
    printf("View cleaned up for reload\n");
    
    // Clean up camera resources
    cleanup_camera();
}

// Function to clear all cached pages
void clear_page_cache() {
    for (int i = 0; i < MAX_PAGE_HISTORY; i++) {
        content_manager.pages[i].loaded = 0;
        content_manager.pages[i].filename[0] = '\0';
        content_manager.pages[i].num_elements = 0;
        content_manager.pages[i].num_modules = 0;
    }
    content_manager.current_page_index = -1;
    content_manager.num_pages_loaded = 0;
    content_manager.current_file_path[0] = '\0';
    printf("Page cache cleared\n");
}

void canvas_render_camera(int x, int y, int width, int height) {
    // Find the canvas UIElement to check its view_mode
    UIElement* current_canvas = NULL;
    for (int i = 0; i < num_elements; i++) {
        if (strcmp(elements[i].type, "canvas") == 0 && elements[i].calculated_width == width && elements[i].calculated_height == height) {
            current_canvas = &elements[i];
            break;
        }
    }

    int is_3d = (current_canvas && strcmp(current_canvas->view_mode, "3d") == 0);

    if (is_3d) {
        // --- 3D Rendering Mode for AR ---
        glEnable(GL_DEPTH_TEST);

        // Setup projection matrix for 3D (use the same as canvas_render_sample)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(current_canvas->fov, (double)width / (double)height, 0.1, 100.0);

        // Setup modelview matrix for camera (use the same as canvas_render_sample)
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        gluLookAt(current_canvas->camera_pos[0], current_canvas->camera_pos[1], current_canvas->camera_pos[2],
                  current_canvas->camera_target[0], current_canvas->camera_target[1], current_canvas->camera_target[2],
                  current_canvas->camera_up[0], current_canvas->camera_up[1], current_canvas->camera_up[2]);

        // Draw camera frame as background texture in 3D space
        if (cam_rgb_data && camera_initialized) {
            // Enable texturing to apply camera feed as background
            glEnable(GL_TEXTURE_2D);
            
            // Create a texture for the camera frame
            GLuint texture_id;
            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            
            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            // Upload camera frame to texture
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cam_width, cam_height, 0, 
                         GL_RGBA, GL_UNSIGNED_BYTE, cam_rgb_data);
            
            // Draw textured quad at the back of the scene to simulate camera background
            // Position at the back of the 3D scene
            glBegin(GL_QUADS);
            // Use normalized coordinates relative to the canvas size
            float scale_factor = 5.0f; // Keep similar to original canvas scale
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-scale_factor, -scale_factor, -5.0f); // Bottom-left (flipped)
            glTexCoord2f(1.0f, 1.0f); glVertex3f(scale_factor, -scale_factor, -5.0f);  // Bottom-right (flipped)
            glTexCoord2f(1.0f, 0.0f); glVertex3f(scale_factor, scale_factor, -5.0f);   // Top-right (flipped)
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-scale_factor, scale_factor, -5.0f);  // Top-left (flipped)
            glEnd();
            
            // Clean up texture
            glDeleteTextures(1, &texture_id);
        } else {
            // Fallback background if no camera data
            glColor4f(0.0f, 0.0f, 0.2f, 1.0f); // Dark blue background
            glBegin(GL_QUADS);
            float scale_factor = 5.0f;
            glVertex3f(-scale_factor, -scale_factor, -5.0f);
            glVertex3f(scale_factor, -scale_factor, -5.0f);
            glVertex3f(scale_factor, scale_factor, -5.0f);
            glVertex3f(-scale_factor, scale_factor, -5.0f);
            glEnd();
        }

        // Initialize camera if not already done
        if (!camera_initialized) {
            init_camera();
            if (!camera_initialized) {
                // If camera initialization failed, still render 3D shapes
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // Reset color
                glDisable(GL_TEXTURE_2D);
                // Restore matrices and state
                glPopMatrix(); // Pop modelview
                glMatrixMode(GL_PROJECTION);
                glPopMatrix(); // Pop projection
                glMatrixMode(GL_MODELVIEW);
                glDisable(GL_DEPTH_TEST);
                return;
            }
        }
        
        // Update camera frame if camera is initialized
        update_camera_frame();

        // Get the shapes from the model for 3D rendering
        int shape_count = 0;
        Shape* shapes = model_get_shapes(&shape_count);

        // Render shapes in 3D space
        for (int i = 0; i < shape_count; i++) {
            Shape* s = &shapes[i];
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_3d_shape(s, width, height);
        }

        glDisable(GL_TEXTURE_2D);

        // Restore matrices and state (like in original canvas_render_sample)
        glPopMatrix(); // Pop modelview
        glMatrixMode(GL_PROJECTION);
        glPopMatrix(); // Pop projection
        glMatrixMode(GL_MODELVIEW);
        glDisable(GL_DEPTH_TEST);
    } else {
        // --- 2D Rendering Mode for AR ---
        // Initialize camera if not already done
        if (!camera_initialized) {
            init_camera();
            if (!camera_initialized) {
                // If camera initialization failed, render a placeholder
                glColor4f(0.2f, 0.2f, 0.2f, 1.0f); // Dark gray background
                glBegin(GL_QUADS);
                glVertex2i(0, 0);
                glVertex2i(width, 0);
                glVertex2i(width, height);
                glVertex2i(0, height);
                glEnd();
                
                // Draw an "X" to indicate no camera
                glColor4f(1.0f, 0.0f, 0.0f, 1.0f); // Red color
                glLineWidth(3.0f);
                glBegin(GL_LINES);
                glVertex2i(10, 10);
                glVertex2i(width - 10, height - 10);
                glVertex2i(width - 10, 10);
                glVertex2i(10, height - 10);
                glEnd();
                glLineWidth(1.0f);
                return;
            }
        }
        
        // Update camera frame if camera is initialized
        update_camera_frame();
        
        // Draw camera frame to canvas as background
        if (cam_rgb_data && camera_initialized) {
            // Calculate scale factors to fit camera frame into canvas
            float cam_aspect = (float)cam_width / (float)cam_height;
            float canvas_aspect = (float)width / (float)height;
            
            float scale_x = 1.0f, scale_y = 1.0f;
            float offset_x = 0.0f, offset_y = 0.0f;
            
            if (cam_aspect > canvas_aspect) {
                // Camera frame is wider relative to canvas - fit width, center height
                scale_x = 1.0f;
                scale_y = (float)width / (float)cam_width * (float)cam_height / (float)height;
                offset_y = (height - height * scale_y) / 2.0f;
            } else {
                // Canvas is wider relative to camera frame - fit height, center width
                scale_y = 1.0f;
                scale_x = (float)height / (float)cam_height * (float)cam_width / (float)width;
                offset_x = (width - width * scale_x) / 2.0f;
            }
            
            glEnable(GL_TEXTURE_2D);
            
            // Create a texture for the camera frame
            GLuint texture_id;
            glGenTextures(1, &texture_id);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            
            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            // Upload camera frame to texture
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cam_width, cam_height, 0, 
                         GL_RGBA, GL_UNSIGNED_BYTE, cam_rgb_data);
            
            // Draw the textured quad with proper scaling and offset
            glBegin(GL_QUADS);
            glTexCoord2f(0.0, 0.0); glVertex2f(offset_x, offset_y + height * scale_y);  // Top-left of texture -> top-left of quad
            glTexCoord2f(1.0, 0.0); glVertex2f(offset_x + width * scale_x, offset_y + height * scale_y);  // Top-right of texture -> top-right of quad
            glTexCoord2f(1.0, 1.0); glVertex2f(offset_x + width * scale_x, offset_y);  // Bottom-right of texture -> bottom-right of quad
            glTexCoord2f(0.0, 1.0); glVertex2f(offset_x, offset_y);  // Bottom-left of texture -> bottom-left of quad
            glEnd();
            
            // Clean up texture
            glDeleteTextures(1, &texture_id);
            glDisable(GL_TEXTURE_2D);
        } else {
            // Fallback if no camera data
            glColor4f(0.0f, 0.0f, 0.5f, 1.0f); // Dark blue background
            glBegin(GL_QUADS);
            glVertex2i(0, 0);
            glVertex2i(width, 0);
            glVertex2i(width, height);
            glVertex2i(0, height);
            glEnd();
        }
        
        // Render shapes from the model on top of the camera feed (this is the augmented reality part)
        // Get the shapes from the model
        int shape_count = 0;
        Shape* shapes = model_get_shapes(&shape_count);

        // Render each shape in 2D on top of the camera feed
        for (int i = 0; i < shape_count; i++) {
            Shape* s = &shapes[i];
            glColor4f(s->color[0], s->color[1], s->color[2], s->alpha);
            render_2d_shape(s);
        }
    }
}


// Function to parse and add inline CHTML content to the DOM
int parse_inline_chtml(const char* chtml_content, int parent_element_index) {
    if (!chtml_content) return 0;
    
    // Create a temporary file to use the existing parse_chtml function
    FILE* temp_file = fopen("temp_inline.chtml", "w");
    if (!temp_file) {
        printf("Error: Could not create temporary file for inline CHTML parsing\n");
        return 0;
    }
    
    fprintf(temp_file, "%s", chtml_content);
    fclose(temp_file);
    
    // Store the current number of elements to know where to start adding new ones
    int original_num_elements = num_elements;
    
    // Parse the temporary file using the existing function
    parse_chtml("temp_inline.chtml");
    
    // The new elements have been added to the elements array
    int new_elements_added = num_elements - original_num_elements;
    
    // Update parent indices for newly added elements if a parent is specified
    if (parent_element_index >= 0 && parent_element_index < MAX_ELEMENTS) {
        for (int i = original_num_elements; i < num_elements; i++) {
            elements[i].parent = parent_element_index;
        }
    }
    
    // Clean up the temporary file
    remove("temp_inline.chtml");
    
    printf("Added %d new elements from inline CHTML\n", new_elements_added);
    
    return new_elements_added;
}


// Function to add CHTML content to the DOM
int add_chtml_to_dom(const char* chtml_content) {
    if (!chtml_content) return 0;
    
    int original_count = num_elements;
    int added = parse_inline_chtml(chtml_content, -1); // No parent specified
    
    // If new elements were added, set flag to refresh display
    if (added > 0) {
        display_needs_refresh = 1;
    }
    
    return added;
}

// Function to remove elements from the DOM
int remove_chtml_from_dom(int start_index, int count) {
    if (start_index < 0 || start_index >= num_elements || count <= 0) {
        return 0;
    }
    
    // Make sure we don't try to remove more elements than exist
    if (start_index + count > num_elements) {
        count = num_elements - start_index;
    }
    
    // Shift elements after the removed section to fill the gap
    for (int i = start_index; i < num_elements - count; i++) {
        elements[i] = elements[i + count];
    }
    
    // Update the element count
    num_elements -= count;
    
    // Adjust parent indices for elements that were shifted
    for (int i = 0; i < num_elements; i++) {
        if (elements[i].parent >= start_index + count) {
            elements[i].parent -= count;
        } else if (elements[i].parent >= start_index) {
            // Parent was removed, set to -1 (no parent)
            elements[i].parent = -1;
        }
    }
    
    printf("Removed %d elements from DOM starting at index %d\n", count, start_index);
    
    return count;
}

// Function to load CHTML from file
char* load_chtml_from_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open CHTML file: %s\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate memory for content
    char* content = (char*)malloc(file_size + 1);
    if (!content) {
        printf("Error: Could not allocate memory for CHTML content from file: %s\n", filename);
        fclose(file);
        return NULL;
    }
    
    // Read file contents
    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';  // Null-terminate
    
    fclose(file);
    
    return content;
}

// Function to create a window/container for inline CHTML
int createWindow(const char* title, const char* chtmlContent, int width, int height) {
    if (num_elements >= MAX_ELEMENTS) {
        printf("Error: Cannot create window, maximum elements reached\n");
        return -1;
    }
    
    // Create a panel element to act as the window container
    int window_index = num_elements;
    strcpy(elements[window_index].type, "panel");
    strcpy(elements[window_index].id, "inline_window");
    strcpy(elements[window_index].label, title);
    elements[window_index].x = 100;  // Default position
    elements[window_index].y = 100;  // Default position
    elements[window_index].width.value = width;
    elements[window_index].width.unit = FLEX_UNIT_PIXEL;
    elements[window_index].height.value = height;
    elements[window_index].height.unit = FLEX_UNIT_PIXEL;
    elements[window_index].calculated_width = width;
    elements[window_index].calculated_height = height;
    elements[window_index].color[0] = 0.2f;  // Dark gray background
    elements[window_index].color[1] = 0.2f;
    elements[window_index].color[2] = 0.2f;
    elements[window_index].alpha = 0.9f;
    elements[window_index].parent = -1;  // Top-level element
    elements[window_index].is_active = 0;
    elements[window_index].is_checked = 0;
    elements[window_index].slider_value = 0;
    elements[window_index].slider_min = 0;
    elements[window_index].slider_max = 100;
    elements[window_index].slider_step = 1;
    elements[window_index].canvas_initialized = 0;
    elements[window_index].canvas_render_func = NULL;
    strcpy(elements[window_index].view_mode, "2d");
    strcpy(elements[window_index].onClick, "");
    elements[window_index].menu_items_count = 0;
    elements[window_index].is_open = 0;
    strcpy(elements[window_index].dir_path, ".");
    elements[window_index].dir_entry_count = 0;
    elements[window_index].dir_entry_selected = -1;
    strcpy(elements[window_index].href, "");
    elements[window_index].z_level = 10;  // Higher z-level for windows
    
    // Initialize flex sizing attributes
    elements[window_index].calculated_x = 0;
    elements[window_index].calculated_y = 0;
    
    num_elements++;
    
    // Now parse and add the CHTML content as children of this window
    if (chtmlContent) {
        // Check if chtmlContent is a file path by checking for .chtml extension
        if (strstr(chtmlContent, ".chtml") != NULL) {
            // Load from file
            char* file_content = load_chtml_from_file(chtmlContent);
            if (file_content) {
                // Parse the loaded CHTML content and add as children of the window
                int original_num_elements = num_elements;
                parse_inline_chtml(file_content, window_index);
                
                // Preload textures for any image elements that were added
                for (int i = original_num_elements; i < num_elements; i++) {
                    if ((strcmp(elements[i].type, "img") == 0 || strcmp(elements[i].type, "image") == 0) && 
                        strlen(elements[i].image_src) > 0) {
                        printf("Loading image for inline element: %s\n", elements[i].image_src);
                        elements[i].texture_id = load_texture_from_file(elements[i].image_src);
                        if (elements[i].texture_id != 0) {
                            elements[i].texture_loaded = 1;
                            elements[i].texture_loading = 0;
                        } else {
                            elements[i].texture_loading = 0; // Mark as not loading if failed
                        }
                    }
                }
                
                printf("Created window '%s' with %d child elements from file: %s\n", 
                       title, num_elements - original_num_elements, chtmlContent);
                
                free(file_content); // Free the loaded content
            } else {
                printf("Failed to load CHTML from file: %s\n", chtmlContent);
            }
        } else {
            // Parse as inline content string
            int original_num_elements = num_elements;
            parse_inline_chtml(chtmlContent, window_index);
            
            // Preload textures for any image elements that were added
            for (int i = original_num_elements; i < num_elements; i++) {
                if ((strcmp(elements[i].type, "img") == 0 || strcmp(elements[i].type, "image") == 0) && 
                    strlen(elements[i].image_src) > 0) {
                    printf("Loading image for inline element: %s\n", elements[i].image_src);
                    elements[i].texture_id = load_texture_from_file(elements[i].image_src);
                    if (elements[i].texture_id != 0) {
                        elements[i].texture_loaded = 1;
                        elements[i].texture_loading = 0;
                    } else {
                        elements[i].texture_loading = 0; // Mark as not loading if failed
                    }
                }
            }
            
            printf("Created window '%s' with %d child elements from inline content\n", 
                   title, num_elements - original_num_elements);
        }
    }
    
    return window_index;
}

// Function to load and display CHTML file in a new window
int load_chtml_file_to_window(const char* filename, int width, int height) {
    if (!filename) return 0;
    
    printf("Loading CHTML file: %s to window %dx%d\n", filename, width, height);
    
    // Create a window with the loaded content
    int window_id = createWindow("Loaded Window", filename, width, height);
    
    // Set the refresh flag to ensure display gets updated
    display_needs_refresh = 1;
    
    return window_id;
}

// Function to check if display needs refresh after DOM updates
int view_needs_refresh() {
    int needs_refresh = display_needs_refresh;
    display_needs_refresh = 0; // Reset the flag after checking
    return needs_refresh;
}

// Function to update inline elements (to be called periodically)
void update_inline_elements() {
    // This function is called periodically to check for new CHTML content
    // from modules and update the DOM accordingly
    // For now, this is a placeholder since it's handled in the model's update_model function
}

