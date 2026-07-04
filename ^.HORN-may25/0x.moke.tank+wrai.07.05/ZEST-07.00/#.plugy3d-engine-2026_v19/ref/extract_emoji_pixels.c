#define STB_IMAGE_IMPLEMENTATION
#include "!.emoji.xtract.stb]c4/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Define the structure for RGB pixels
typedef struct {
    unsigned char r, g, b;
} RGB_Pixel;

// Function to downscale a section of an image to 8x8 pixels with better sampling
void downscale_to_8x8(unsigned char* src_pixels, int src_width, int src_height, int channels,
                      RGB_Pixel dst_pixels[8][8]) {
    float x_ratio = (float)src_width / 8.0f;
    float y_ratio = (float)src_height / 8.0f;
    
    // Calculate source region size for each destination pixel
    int region_width = (int)x_ratio;
    int region_height = (int)y_ratio;
    
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            // Calculate the region in source image that maps to this dest pixel
            int start_x = (int)(x * x_ratio);
            int start_y = (int)(y * y_ratio);
            
            // Determine actual region size, accounting for boundaries
            int end_x = (int)((x + 1) * x_ratio);
            int end_y = (int)((y + 1) * y_ratio);
            
            // Ensure we don't exceed image boundaries
            if (end_x > src_width) end_x = src_width;
            if (end_y > src_height) end_y = src_height;
            if (start_x >= src_width) start_x = src_width - 1;
            if (start_y >= src_height) start_y = src_height - 1;
            
            // Accumulate RGB values from the source region
            long sum_r = 0, sum_g = 0, sum_b = 0;
            int count = 0;
            
            for (int sy = start_y; sy < end_y; sy++) {
                for (int sx = start_x; sx < end_x; sx++) {
                    int src_idx = (sy * src_width + sx) * channels;
                    sum_r += src_pixels[src_idx];
                    sum_g += src_pixels[src_idx + 1];
                    sum_b += src_pixels[src_idx + 2];
                    count++;
                }
            }
            
            // Average the values to get the final pixel
            if (count > 0) {
                dst_pixels[y][x].r = (unsigned char)(sum_r / count);
                dst_pixels[y][x].g = (unsigned char)(sum_g / count);
                dst_pixels[y][x].b = (unsigned char)(sum_b / count);
            } else {
                // Fallback to nearest neighbor if something went wrong
                int src_x = start_x;
                int src_y = start_y;
                int src_idx = (src_y * src_width + src_x) * channels;
                dst_pixels[y][x].r = src_pixels[src_idx];
                dst_pixels[y][x].g = src_pixels[src_idx + 1];
                dst_pixels[y][x].b = src_pixels[src_idx + 2];
            }
        }
    }
}

// Function to write 8x8 pixel data to a CSV file
int write_csv(const char* filename, RGB_Pixel pixels[8][8]) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return 0;
    }
    
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            fprintf(file, "\"%d,%d,%d\"", pixels[y][x].r, pixels[y][x].g, pixels[y][x].b);
            if (x < 7) fprintf(file, ",");
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return 1;
}

// Extract a single emoji from the atlas and create a CSV file
int extract_single_emoji(unsigned char* atlas_data, int atlas_width, int atlas_height, int channels,
                         int emoji_index, int emoji_size, const char* emoji_name) {
    // For the single-row atlas, all emojis are in the first row
    int atlas_x = emoji_index * emoji_size;
    int atlas_y = 0;  // All emojis are in the first row
    
    // Check bounds
    if (atlas_x + emoji_size > atlas_width || atlas_y + emoji_size > atlas_height) {
        fprintf(stderr, "Error: Emoji index %d out of bounds (x=%d, atlas_width=%d)\n", emoji_index, atlas_x, atlas_width);
        return 0;
    }
    
    // Allocate memory for the emoji sub-image
    unsigned char* emoji_pixels = malloc(emoji_size * emoji_size * channels);
    if (!emoji_pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 0;
    }
    
    // Extract the emoji from the atlas
    for (int y = 0; y < emoji_size; y++) {
        for (int x = 0; x < emoji_size; x++) {
            int src_idx = ((atlas_y + y) * atlas_width + (atlas_x + x)) * channels;
            int dst_idx = (y * emoji_size + x) * channels;
            
            // Copy the pixel data
            for (int c = 0; c < channels; c++) {
                emoji_pixels[dst_idx + c] = atlas_data[src_idx + c];
            }
        }
    }
    
    // Create 8x8 downsampled version
    RGB_Pixel downsampled[8][8];
    downscale_to_8x8(emoji_pixels, emoji_size, emoji_size, channels, downsampled);
    
    // Create output directory and file name
    char dir_path[2048];
    snprintf(dir_path, sizeof(dir_path), "%s", emoji_name);
    
    // Create directory if it doesn't exist
    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dir_path);
    system(mkdir_cmd);
    
    // Write to CSV file
    char csv_path[2048];
    snprintf(csv_path, sizeof(csv_path), "%s/%s.csv", dir_path, emoji_name);
    
    int success = write_csv(csv_path, downsampled);
    
    // Clean up
    free(emoji_pixels);
    
    if (success) {
        printf("Successfully extracted emoji %d to %s\n", emoji_index, csv_path);
    }
    
    return success;
}

// Process the entire emoji list file
int process_emoji_list(const char* atlas_filename, const char* list_filename) {
    // Load the atlas image
    int atlas_width, atlas_height, atlas_channels;
    unsigned char* atlas_data = stbi_load(atlas_filename, &atlas_width, &atlas_height, &atlas_channels, 0);
    
    if (!atlas_data) {
        fprintf(stderr, "Error: Could not load atlas image %s\n", atlas_filename);
        return 0;
    }
    
    printf("Loaded atlas: %dx%d with %d channels\n", atlas_width, atlas_height, atlas_channels);
    
    // Open the emoji list file
    FILE* list_file = fopen(list_filename, "r");
    if (!list_file) {
        fprintf(stderr, "Error: Could not open emoji list file %s\n", list_filename);
        stbi_image_free(atlas_data);
        return 0;
    }
    
    char emoji_line[1024];  // Increase buffer size for longer lines
    int total_emojis = 0;
    
    // First, count the total number of emojis by skipping header lines
    long pos = ftell(list_file);
    char temp_line[1024];
    while (fgets(temp_line, sizeof(temp_line), list_file)) {
        if (temp_line[0] != '#') {  // Not a comment line
            total_emojis++;
        }
    }
    fseek(list_file, pos, SEEK_SET);  // Reset to beginning to process
    
    printf("Processing %d emojis from the list\n", total_emojis);
    
    int emoji_index = 0;
    while (fgets(emoji_line, sizeof(emoji_line), list_file)) {
        // Remove newline characters
        emoji_line[strcspn(emoji_line, "\r\n")] = 0;
        
        // Skip comment lines
        if (strlen(emoji_line) == 0 || emoji_line[0] == '#') {
            continue;
        }
        
        // Parse the line: codepoint_hex_string|emoji_string|emoji_name
        char *token = strtok(emoji_line, "|");
        if (!token) continue;
        
        // Get emoji string (second part)
        char *emoji_str = strtok(NULL, "|");
        if (!emoji_str) continue;
        
        // Get emoji name (third part)
        char *emoji_name_raw = strtok(NULL, "|");
        if (!emoji_name_raw) {
            // If no name provided, just use the index
            emoji_name_raw = "unnamed";
        }
        
        // Create a safe directory name by sanitizing the emoji name
        char emoji_name[256];
        int j = 0;
        for (int i = 0; emoji_name_raw[i] && j < 254; i++) {
            if ((emoji_name_raw[i] >= 'a' && emoji_name_raw[i] <= 'z') ||
                (emoji_name_raw[i] >= 'A' && emoji_name_raw[i] <= 'Z') ||
                (emoji_name_raw[i] >= '0' && emoji_name_raw[i] <= '9') ||
                emoji_name_raw[i] == '_' || emoji_name_raw[i] == '-') {
                emoji_name[j++] = emoji_name_raw[i];
            } else {
                emoji_name[j++] = '_';
            }
        }
        emoji_name[j] = '\0';
        
        // If no valid name characters, use index
        if (j == 0) {
            snprintf(emoji_name, sizeof(emoji_name), "emoji_%d", emoji_index);
        }
        
        // For the single-row atlas, y position is always 0, x is calculated by index
        // Extract the emoji and write to CSV
        if (!extract_single_emoji(atlas_data, atlas_width, atlas_height, atlas_channels,
                                  emoji_index, 64, emoji_name)) {
            fprintf(stderr, "Failed to extract emoji at index %d\n", emoji_index);
        }
        
        emoji_index++;
        
        // Print progress every 100 emojis
        if (emoji_index % 100 == 0) {
            printf("Processed %d emojis...\n", emoji_index);
        }
    }
    
    // Clean up
    fclose(list_file);
    stbi_image_free(atlas_data);
    
    printf("Successfully processed %d emojis\n", emoji_index);
    return 1;
}



// Process a single emoji by name from the list file

int process_single_emoji_by_name(const char* atlas_filename, const char* list_filename, const char* target_emoji_name) {

    // Load the atlas image

    int atlas_width, atlas_height, atlas_channels;

    unsigned char* atlas_data = stbi_load(atlas_filename, &atlas_width, &atlas_height, &atlas_channels, 0);



    if (!atlas_data) {

        fprintf(stderr, "Error: Could not load atlas image %s\n", atlas_filename);

        return 0;

    }



    // Open the emoji list file

    FILE* list_file = fopen(list_filename, "r");

    if (!list_file) {

        fprintf(stderr, "Error: Could not open emoji list file %s\n", list_filename);

        stbi_image_free(atlas_data);

        return 0;

    }



    char emoji_line[1024];

    int emoji_index = 0;

    int found = 0;



    while (fgets(emoji_line, sizeof(emoji_line), list_file)) {

        // Remove newline characters

        emoji_line[strcspn(emoji_line, "\r\n")] = 0;



        // Skip comment lines

        if (strlen(emoji_line) == 0 || emoji_line[0] == '#') {

            continue;

        }



        // Create a mutable copy of the line for strtok

        char mutable_line[1024];

        strcpy(mutable_line, emoji_line);



        // Parse the line: codepoint_hex_string|emoji_string|emoji_name

        char *token = strtok(mutable_line, "|");

        if (!token) continue;



        char *emoji_str = strtok(NULL, "|");

        if (!emoji_str) continue;



        char *emoji_name_raw = strtok(NULL, "|");

        if (!emoji_name_raw) continue;

        

        // Sanitize the name to match the format used in extraction

        char sanitized_name[256];

        int j = 0;

        for (int i = 0; emoji_name_raw[i] && j < 254; i++) {

            if ((emoji_name_raw[i] >= 'a' && emoji_name_raw[i] <= 'z') ||

                (emoji_name_raw[i] >= 'A' && emoji_name_raw[i] <= 'Z') ||

                (emoji_name_raw[i] >= '0' && emoji_name_raw[i] <= '9') ||

                emoji_name_raw[i] == '_' || emoji_name_raw[i] == '-') {

                sanitized_name[j++] = emoji_name_raw[i];

            } else {

                sanitized_name[j++] = '_';

            }

        }

        sanitized_name[j] = '\0';



        if (strcmp(sanitized_name, target_emoji_name) == 0) {

            printf("Found emoji '%s' at index %d\n", target_emoji_name, emoji_index);

            if (!extract_single_emoji(atlas_data, atlas_width, atlas_height, atlas_channels,

                                      emoji_index, 64, sanitized_name)) {

                fprintf(stderr, "Failed to extract emoji at index %d\n", emoji_index);

            }

            found = 1;

            break; 

        }



        emoji_index++;

    }



    fclose(list_file);

    stbi_image_free(atlas_data);



    if (!found) {

        fprintf(stderr, "Error: Emoji '%s' not found in list file.\n", target_emoji_name);

        return 0;

    }



    return 1;

}



int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <atlas.png> <list.txt> <emoji_name|--all>\n", argv[0]);
        return 1;
    }

    const char* atlas_filename = argv[1];
    const char* list_filename = argv[2];
    const char* command = argv[3];

    if (strcmp(command, "--all") == 0) {
        printf("Extracting all emoji pixels from %s using %s\n", atlas_filename, list_filename);
        if (!process_emoji_list(atlas_filename, list_filename)) {
            fprintf(stderr, "Process failed\n");
            return 1;
        }
    } else {
        printf("Extracting single emoji '%s' from %s using %s\n", command, atlas_filename, list_filename);
        if (!process_single_emoji_by_name(atlas_filename, list_filename, command)) {
            fprintf(stderr, "Process failed\n");
            return 1;
        }
    }
    
    printf("Emoji extraction completed successfully!\n");
    return 0;
}
