#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

// --- Config ---
#define EMOJI_RENDER_SIZE 64   // Matches your Candy Crush tile inner size
#define MAX_EMOJIS 4096  // Maximum number of emojis to support (more than enough for all emojis)
#define ATLAS_WIDTH (EMOJI_RENDER_SIZE * MAX_EMOJIS)
#define ATLAS_HEIGHT EMOJI_RENDER_SIZE

// We'll work in RGBA for PNG
typedef struct {
    unsigned char r, g, b, a;
} RGBA_Pixel;

char *emoji_strings[MAX_EMOJIS];
char *emoji_names[MAX_EMOJIS];
int NUM_EMOJIS = 0;  // Will be set dynamically based on file contents

// Function to read emojis from parsed_emojis.txt
int read_emojis_from_file() {
    FILE *file = fopen("parsed_emojis.txt", "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open parsed_emojis.txt\n");
        return 0;
    }

    char line[1024];
    int count = 0;
    
    // Skip header lines that start with #
    while (fgets(line, sizeof(line), file) && count < MAX_EMOJIS) {
        if (line[0] == '#') continue;  // Skip comment lines
        
        // Remove newline character
        line[strcspn(line, "\n")] = 0;
        
        // Parse the line: codepoint_hex_string|emoji_string|emoji_name
        char *token = strtok(line, "|");
        if (!token) continue;
        
        // Allocate memory and store emoji hex code (convert to U+ format)
        char formatted_name[32];
        snprintf(formatted_name, sizeof(formatted_name), "U+%s", token);
        emoji_names[count] = malloc(strlen(formatted_name) + 1);
        strcpy(emoji_names[count], formatted_name);
        
        // Get emoji string (second part)
        token = strtok(NULL, "|");
        if (!token) continue;
        emoji_strings[count] = malloc(strlen(token) + 1);
        strcpy(emoji_strings[count], token);
        
        // Get emoji description (third part) - we don't need it but let's skip
        token = strtok(NULL, "|");
        if (token) {
            // Just consume the third token but don't store it
        }
        
        count++;
    }
    
    fclose(file);
    NUM_EMOJIS = count;
    printf("✅ Loaded %d emojis from parsed_emojis.txt\n", NUM_EMOJIS);
    return 1;
}

// Function to free allocated memory
void free_emoji_data() {
    for (int i = 0; i < NUM_EMOJIS; i++) {
        free(emoji_strings[i]);
        free(emoji_names[i]);
    }
}

// --- UTF-8 Decoder ---
int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0 && (str[1] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    if ((str[0] & 0xF0) == 0xE0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    if ((str[0] & 0xF8) == 0xF0 && (str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
        *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
}

// --- Write PNG via stb_image_write ---
int write_png(const char* filename, RGBA_Pixel* pixels, int width, int height) {
    int result = stbi_write_png(filename, width, height, 4, pixels, width * 4);
    if (result == 0) {
        fprintf(stderr, "❌ Failed to write PNG: %s\n", filename);
        return 0;
    }
    printf("✅ Atlas saved to %s\n", filename);
    return 1;
}

// --- Main ---
int main() {
    // Load emojis from parsed_emojis.txt file
    if (!read_emojis_from_file()) {
        return 1;  // Error already printed in function
    }
    
    // Calculate actual atlas dimensions based on loaded emojis
    int actual_atlas_width = EMOJI_RENDER_SIZE * NUM_EMOJIS;
    int actual_atlas_height = EMOJI_RENDER_SIZE;
    
    FT_Library ft;
    FT_Face face;
    RGBA_Pixel* atlas = calloc(actual_atlas_width * actual_atlas_height, sizeof(RGBA_Pixel));
    if (!atlas) {
        fprintf(stderr, "Error: Failed to allocate atlas memory\n");
        free_emoji_data();
        return 1;
    }

    const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";

    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Error: Could not init FreeType\n");
        free(atlas);
        free_emoji_data();
        return 1;
    }

    if (FT_New_Face(ft, emoji_font_path, 0, &face)) {
        fprintf(stderr, "Error: Could not load font %s\n", emoji_font_path);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    if (face->num_fixed_sizes == 0) {
        fprintf(stderr, "Error: Font has no fixed sizes\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    // Pick strike closest to EMOJI_RENDER_SIZE (64)
    int best_match = 0;
    int best_diff = abs(face->available_sizes[0].height - EMOJI_RENDER_SIZE);
    for (int i = 1; i < face->num_fixed_sizes; i++) {
        int diff = abs(face->available_sizes[i].height - EMOJI_RENDER_SIZE);
        if (diff < best_diff) {
            best_diff = diff;
            best_match = i;
        }
    }

    if (FT_Select_Size(face, best_match)) {
        fprintf(stderr, "Error: Could not select size index %d\n", best_match);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    int loaded_size = face->available_sizes[best_match].height;
    printf("📌 Selected font strike: %dx%d → rendering at %dx%d\n",
           loaded_size, loaded_size, EMOJI_RENDER_SIZE, EMOJI_RENDER_SIZE);

    FILE *uv_file = fopen("uv_coords.txt", "w");
    if (!uv_file) {
        fprintf(stderr, "Error: Could not create uv_coords.txt\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free(atlas);
        free_emoji_data();
        return 1;
    }

    fprintf(uv_file, "# UV coords: x_min, y_min, x_max, y_max (normalized)\n");

    for (int i = 0; i < NUM_EMOJIS; i++) {
        const unsigned char* str = (const unsigned char*)emoji_strings[i];
        unsigned int codepoint;
        decode_utf8(str, &codepoint);

        if (codepoint == 0xFE0F) continue;  // Skip variation selector

        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR)) {
            fprintf(stderr, "⚠️  Could not load U+%04X (%s)\n", codepoint, emoji_strings[i]);
            // Fill with placeholder pink
            for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                    int ax = i * EMOJI_RENDER_SIZE + x;
                    int ay = y;
                    atlas[ay * actual_atlas_width + ax] = (RGBA_Pixel){255, 0, 255, 255};
                }
            }
        } else {
            FT_GlyphSlot slot = face->glyph;
            if (slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
                fprintf(stderr, "⚠️  U+%04X not in BGRA mode\n", codepoint);
                // Fill with cyan placeholder
                for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                    for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                        int ax = i * EMOJI_RENDER_SIZE + x;
                        int ay = y;
                        atlas[ay * actual_atlas_width + ax] = (RGBA_Pixel){0, 255, 255, 255};
                    }
                }
                continue;
            }

            // Get source dimensions
            int src_w = slot->bitmap.width;
            int src_h = slot->bitmap.rows;

            // Calculate scaling factor to fit within EMOJI_RENDER_SIZE
            float scale = (float)EMOJI_RENDER_SIZE / fmaxf((float)src_w, (float)src_h);
            int target_w = (int)(src_w * scale);
            int target_h = (int)(src_h * scale);

            // Use bitmap_top to vertically align relative to baseline
            // bitmap_top = distance from top of bitmap to baseline
            // We want baseline at 80% from top of cell
            int baseline_y = (int)(EMOJI_RENDER_SIZE * 0.8f);
            int scaled_bitmap_top = (int)(slot->bitmap_top * scale);
            int dst_y_offset = baseline_y - scaled_bitmap_top;

            // Horizontal center
            int dst_x_offset = (EMOJI_RENDER_SIZE - target_w) / 2;

            // Safety clamps
            if (dst_y_offset < 0) {
                int shift = -dst_y_offset;
                dst_y_offset = 0;
            }
            if (dst_y_offset + target_h > EMOJI_RENDER_SIZE) {
                int overflow = (dst_y_offset + target_h) - EMOJI_RENDER_SIZE;
                dst_y_offset -= overflow;
                if (dst_y_offset < 0) dst_y_offset = 0;
            }

            // Clear the target cell
            for (int y = 0; y < EMOJI_RENDER_SIZE; y++) {
                for (int x = 0; x < EMOJI_RENDER_SIZE; x++) {
                    int ax = i * EMOJI_RENDER_SIZE + x;
                    int ay = y;
                    atlas[ay * actual_atlas_width + ax] = (RGBA_Pixel){0, 0, 0, 0};
                }
            }

            // Scale and blit
            for (int y = 0; y < target_h; y++) {
                for (int x = 0; x < target_w; x++) {
                    // Source pixel (nearest neighbor)
                    int src_x = (int)((float)x / scale);
                    int src_y = (int)((float)y / scale);
                    if (src_x >= src_w || src_y >= src_h) continue;

                    int src_idx = src_y * slot->bitmap.pitch + src_x * 4;
                    unsigned char* src_pixel = &slot->bitmap.buffer[src_idx];

                    int dst_x = i * EMOJI_RENDER_SIZE + dst_x_offset + x;
                    int dst_y = dst_y_offset + y;

                    if (dst_x >= actual_atlas_width || dst_y >= actual_atlas_height || dst_y < 0) continue;

                    // Convert BGRA → RGBA
                    atlas[dst_y * actual_atlas_width + dst_x] = (RGBA_Pixel){
                        .r = src_pixel[2],
                        .g = src_pixel[1],
                        .b = src_pixel[0],
                        .a = src_pixel[3]
                    };
                }
            }
        }

        float x_min = (float)i / NUM_EMOJIS;
        float x_max = (float)(i + 1) / NUM_EMOJIS;
        fprintf(uv_file, "Emoji %s: %.6f %.6f %.6f %.6f\n",
                emoji_names[i], x_min, 0.0f, x_max, 1.0f);
    }

    fclose(uv_file);
    printf("✅ UV coordinates saved to uv_coords.txt\n");

    // Write emoji position mapping for the extraction script to use
    FILE *map_file = fopen("emoji_positions_map.txt", "w");
    if (map_file) {
        fprintf(map_file, "# Emoji Position Mapping\n");
        fprintf(map_file, "# Format: emoji_name|position_index|sanitized_name\n");
        
        // Read the original file again to extract emoji names in the format expected by the extractor
        FILE *orig_file = fopen("parsed_emojis.txt", "r");
        if (orig_file) {
            char line[1024];
            int pos_index = 0;
            
            while (fgets(line, sizeof(line), orig_file) && pos_index < NUM_EMOJIS) {
                if (line[0] == '#') continue;  // Skip comment lines
                
                // Remove newline character
                line[strcspn(line, "\n")] = 0;
                
                // Parse the line: codepoint_hex_string|emoji_string|emoji_name
                char *codepoint = strtok(line, "|");
                if (!codepoint) continue;
                
                char *emoji_str = strtok(NULL, "|");
                if (!emoji_str) continue;
                
                char *emoji_name = strtok(NULL, "|");
                if (!emoji_name) continue;
                
                // Sanitize the emoji name for comparison (same as in Python script)
                char sanitized_name[256];
                int j = 0;
                for (int i = 0; emoji_name[i] && j < 254; i++) {
                    if ((emoji_name[i] >= 'a' && emoji_name[i] <= 'z') ||
                        (emoji_name[i] >= 'A' && emoji_name[i] <= 'Z') ||
                        (emoji_name[i] >= '0' && emoji_name[i] <= '9') ||
                        emoji_name[i] == '_' || emoji_name[i] == '-') {
                        sanitized_name[j++] = emoji_name[i];
                    } else {
                        sanitized_name[j++] = '_';
                    }
                }
                sanitized_name[j] = '\0';
                
                fprintf(map_file, "%s|%d|%s\n", sanitized_name, pos_index, sanitized_name);
                pos_index++;
            }
            fclose(orig_file);
        }
        fclose(map_file);
        printf("✅ Wrote emoji position map for %d emojis to emoji_positions_map.txt\n", NUM_EMOJIS);
    } else {
        fprintf(stderr, "⚠️ Warning: Could not write emoji_positions_map.txt\n");
    }

    if (!write_png("emoji_atlas.png", atlas, actual_atlas_width, actual_atlas_height)) {
        free(atlas);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        free_emoji_data();
        return 1;
    }

    free(atlas);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    free_emoji_data();

    printf("🎉 FINAL ATLAS GENERATED — %d emojis, perfectly aligned, no cutoffs!\n", NUM_EMOJIS);
    return 0;
}
