#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <GL/glut.h>

// Forward declarations for model functions that raycasting needs
extern int get_canvas_rows(void);
extern int get_canvas_cols(void);
extern int get_tile_size(void);
extern int get_max_layers(void);
extern int get_current_z_level(void);
extern int get_canvas_tile(int layer, int r, int c, int field);
extern float get_scale_x(void);
extern float get_scale_y(void);
extern float get_scale_z(void);

// Raycasting function declarations
void screen_to_world_ray(int screen_x, int screen_y, float* ray_origin, float* ray_direction);
int ray_intersects_box(float ray_origin[3], float ray_direction[3], 
                      float box_min[3], float box_max[3], float* distance);
int find_closest_intersected_block(int screen_x, int screen_y, int* out_row, int* out_col, int* out_layer);

// Raycasting functions for 3D block selection

// Convert screen coordinates to world coordinates for raycasting
void screen_to_world_ray(int screen_x, int screen_y, float* ray_origin, float* ray_direction) {
    // Get viewport dimensions
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Get projection and modelview matrices
    GLdouble modelview[16];
    GLdouble projection[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    
    // Convert screen coordinates to world coordinates
    GLdouble wx_near, wy_near, wz_near;
    GLdouble wx_far, wy_far, wz_far;
    
    // Near plane point (z = 0)
    gluUnProject(screen_x, viewport[3] - screen_y, 0.0,
                 modelview, projection, viewport,
                 &wx_near, &wy_near, &wz_near);
    
    // Far plane point (z = 1)
    gluUnProject(screen_x, viewport[3] - screen_y, 1.0,
                 modelview, projection, viewport,
                 &wx_far, &wy_far, &wz_far);
    
    // Ray origin is the near point
    ray_origin[0] = (float)wx_near;
    ray_origin[1] = (float)wy_near;
    ray_origin[2] = (float)wz_near;
    
    // Ray direction is the vector from near to far point
    ray_direction[0] = (float)(wx_far - wx_near);
    ray_direction[1] = (float)(wy_far - wy_near);
    ray_direction[2] = (float)(wz_far - wz_near);
    
    // Normalize the direction vector
    float length = sqrt(ray_direction[0]*ray_direction[0] + 
                       ray_direction[1]*ray_direction[1] + 
                       ray_direction[2]*ray_direction[2]);
    if (length > 0.0f) {
        ray_direction[0] /= length;
        ray_direction[1] /= length;
        ray_direction[2] /= length;
    }
}

// Ray-box intersection test
int ray_intersects_box(float ray_origin[3], float ray_direction[3], 
                      float box_min[3], float box_max[3], float* distance) {
    float tmin = -1e30f, tmax = 1e30f;
    
    // Check X slab
    if (fabs(ray_direction[0]) < 1e-8f) {
        // Ray is parallel to X planes
        if (ray_origin[0] < box_min[0] || ray_origin[0] > box_max[0])
            return 0; // No intersection
    } else {
        float inv_dir = 1.0f / ray_direction[0];
        float t1 = (box_min[0] - ray_origin[0]) * inv_dir;
        float t2 = (box_max[0] - ray_origin[0]) * inv_dir;
        
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        
        tmin = fmax(tmin, t1);
        tmax = fmin(tmax, t2);
        
        if (tmin > tmax)
            return 0; // No intersection
    }
    
    // Check Y slab
    if (fabs(ray_direction[1]) < 1e-8f) {
        // Ray is parallel to Y planes
        if (ray_origin[1] < box_min[1] || ray_origin[1] > box_max[1])
            return 0; // No intersection
    } else {
        float inv_dir = 1.0f / ray_direction[1];
        float t1 = (box_min[1] - ray_origin[1]) * inv_dir;
        float t2 = (box_max[1] - ray_origin[1]) * inv_dir;
        
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        
        tmin = fmax(tmin, t1);
        tmax = fmin(tmax, t2);
        
        if (tmin > tmax)
            return 0; // No intersection
    }
    
    // Check Z slab
    if (fabs(ray_direction[2]) < 1e-8f) {
        // Ray is parallel to Z planes
        if (ray_origin[2] < box_min[2] || ray_origin[2] > box_max[2])
            return 0; // No intersection
    } else {
        float inv_dir = 1.0f / ray_direction[2];
        float t1 = (box_min[2] - ray_origin[2]) * inv_dir;
        float t2 = (box_max[2] - ray_origin[2]) * inv_dir;
        
        if (t1 > t2) {
            float temp = t1;
            t1 = t2;
            t2 = temp;
        }
        
        tmin = fmax(tmin, t1);
        tmax = fmin(tmax, t2);
        
        if (tmin > tmax)
            return 0; // No intersection
    }
    
    // Intersection found
    if (distance)
        *distance = tmin > 0 ? tmin : tmax;
    return 1;
}

// Find the closest intersected block in the 3D canvas
int find_closest_intersected_block(int screen_x, int screen_y, int* out_row, int* out_col, int* out_layer) {
    // Get ray from screen coordinates
    float ray_origin[3];
    float ray_direction[3];
    screen_to_world_ray(screen_x, screen_y, ray_origin, ray_direction);
    
    // Debug output
    fprintf(stderr, "Ray origin: (%f, %f, %f), direction: (%f, %f, %f)\
", 
            ray_origin[0], ray_origin[1], ray_origin[2],
            ray_direction[0], ray_direction[1], ray_direction[2]);
    
    // Variables to track the closest intersection
    float closest_distance = 1e30f;
    int found_intersection = 0;
    int closest_row = -1;
    int closest_col = -1;
    int closest_layer = -1;
    
    // Check for intersection with blocks first
    int block_count = 0;
    for (int layer = 0; layer < get_max_layers(); layer++) {
        for (int row = 0; row < get_canvas_rows(); row++) {
            for (int col = 0; col < get_canvas_cols(); col++) {
                // Check if there's a block at this position
                if (get_canvas_tile(layer, row, col, 0) != -1) {
                    block_count++;
                    // Define the bounding box for this block with scaling
                    float scale_x = get_scale_x();
                    float scale_y = get_scale_y();
                    float scale_z = get_scale_z();
                    float box_min[3] = {
                        col * get_tile_size() * scale_x,
                        row * get_tile_size() * scale_y,
                        layer * get_tile_size() * scale_z
                    };
                    float box_max[3] = {
                        (col + 1) * get_tile_size() * scale_x,
                        (row + 1) * get_tile_size() * scale_y,
                        (layer + 1) * get_tile_size() * scale_z
                    };
                    
                    // Debug output for first few blocks
                    if (block_count <= 5) {
                        fprintf(stderr, "Block %d at (%d, %d, %d) with bounding box min: (%f, %f, %f) max: (%f, %f, %f)\
", 
                                block_count, col, row, layer,
                                box_min[0], box_min[1], box_min[2],
                                box_max[0], box_max[1], box_max[2]);
                    }
                    
                    // Check if ray intersects this block
                    float distance;
                    if (ray_intersects_box(ray_origin, ray_direction, box_min, box_max, &distance)) {
                        // Debug output
                        if (block_count <= 5) {
                            fprintf(stderr, "Ray intersects block %d at distance %f\
", block_count, distance);
                        }
                        
                        // If this is closer than the previous closest intersection
                        if (distance < closest_distance && distance > 0) {
                            closest_distance = distance;
                            closest_row = row;
                            closest_col = col;
                            closest_layer = layer;
                            found_intersection = 1;
                            
                            // Debug output
                            if (block_count <= 5) {
                                fprintf(stderr, "New closest intersection found at block %d\
", block_count);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Debug output
    fprintf(stderr, "Total blocks: %d, found intersection: %d\
", block_count, found_intersection);
    
    // Return the closest intersection if found
    if (found_intersection) {
        *out_row = closest_row;
        *out_col = closest_col;
        *out_layer = closest_layer;
        fprintf(stderr, "Returning intersection at (%d, %d, %d)\
", closest_col, closest_row, closest_layer);
        return 1;
    }
    
    // If no intersection found with blocks, check for intersection with the grid at the current Z-level
    // This allows clicking on empty space to place a block
    float scale_z = get_scale_z();
    float z_plane = get_current_z_level() * get_tile_size() * scale_z;
    
    // Check if ray is parallel to the Z-plane (would cause division by zero)
    if (fabs(ray_direction[2]) > 1e-6f) {
        float t = (z_plane - ray_origin[2]) / ray_direction[2];
        if (t > 0) {
            float world_x = ray_origin[0] + t * ray_direction[0];
            float world_y = ray_origin[1] + t * ray_direction[1];
            
            // Convert world coordinates to grid coordinates with scaling
            float scale_x = get_scale_x();
            float scale_y = get_scale_y();
            int col = (int)(world_x / (get_tile_size() * scale_x));
            int row = (int)(world_y / (get_tile_size() * scale_y));
            
            // Check if the intersection point is within the grid bounds
            if (col >= 0 && col < get_canvas_cols() && row >= 0 && row < get_canvas_rows()) {
                *out_row = row;
                *out_col = col;
                *out_layer = get_current_z_level();
                fprintf(stderr, "Returning grid intersection at (%d, %d, %d)\
", col, row, get_current_z_level());
                return 1;
            }
        }
    }
    
    fprintf(stderr, "No intersection found\
");
    return 0;
}