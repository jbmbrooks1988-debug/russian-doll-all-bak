#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

// Structure to define a range of Unicode code points
typedef struct {
    uint32_t start;
    uint32_t end;
} unicode_range;

// Known Unicode blocks that contain mostly emojis
// Source: Unicode Standard Annex #51 (emoji ranges)
const unicode_range emoji_ranges[] = {
    { 0x1F600, 0x1F64F }, // Emoticons
    { 0x1F300, 0x1F5FF }, // Misc Symbols and Pictographs
    { 0x1F680, 0x1F6FF }, // Transport and Map
    { 0x1F700, 0x1F77F }, // Alchemical Symbols (some emojis)
    { 0x1F780, 0x1F7FF }, // Geometric Shapes (extended)
    { 0x1F800, 0x1F8FF }, // Supplemental Arrows-C
    { 0x1F900, 0x1F9FF }, // Supplemental Symbols and Pictographs
    { 0x1FA00, 0x1FA6F }, // Chess Symbols (some used)
    { 0x1FA70, 0x1FAFF }, // Symbols and Pictographs Extended-A
    { 0x1F000, 0x1F02F }, // Mahjong, Dominoes
    { 0x2600,  0x26FF  }, // Misc Symbols
    { 0x2700,  0x27BF  }, // Dingbats
};

#define NUM_RANGES (sizeof(emoji_ranges) / sizeof(emoji_ranges[0]))

// Helper: Encode a Unicode code point into UTF-8
// Returns number of bytes written (1 to 4)
int unicode_to_utf8(char* buffer, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        buffer[0] = (char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        buffer[0] = 0xC0 | (codepoint >> 6);
        buffer[1] = 0x80 | (codepoint & 0x3F);
        return 2;
    } else if (codepoint <= 0xFFFF) {
        buffer[0] = 0xE0 | (codepoint >> 12);
        buffer[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[2] = 0x80 | (codepoint & 0x3F);
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        buffer[0] = 0xF0 | (codepoint >> 18);
        buffer[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        buffer[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        buffer[3] = 0x80 | (codepoint & 0x3F);
        return 4;
    }

    // Invalid code point
    return 0;
}

// Pick a random valid emoji code point from the ranges
char* get_random_emoji(char* buffer) {
    // Total number of code points across all emoji ranges
    uint32_t total_emojis = 0;
    for (int i = 0; i < NUM_RANGES; i++) {
        total_emojis += emoji_ranges[i].end - emoji_ranges[i].start + 1;
    }

    // Pick a random offset within total emoji space
    uint32_t target = rand() % total_emojis;
    uint32_t count = 0;

    for (int i = 0; i < NUM_RANGES; i++) {
        uint32_t range_size = emoji_ranges[i].end - emoji_ranges[i].start + 1;
        if (target < count + range_size) {
            uint32_t codepoint = emoji_ranges[i].start + (target - count);
            int len = unicode_to_utf8(buffer, codepoint);
            buffer[len] = '\0'; // null terminate
            return buffer;
        }
        count += range_size;
    }

    // Fallback: neutral face
    strcpy(buffer, "😐");
    return buffer;
}

// Check if path is directory
int is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

// Append emoji to state.txt
void append_emoji_to_file(const char* filepath) {
    FILE* file = fopen(filepath, "a");
    if (!file) {
        perror("fopen append");
        return;
    }

    char emoji_utf8[5]; // max 4 bytes + null
    get_random_emoji(emoji_utf8);

    fprintf(file, "symbol : %s\n", emoji_utf8);
    fclose(file);

    printf("Updated: %s with emoji %s\n", filepath, emoji_utf8);
}

// Recursive search
void search_directory(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    char full_path[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_REG && strcmp(entry->d_name, "state.txt") == 0) {
            append_emoji_to_file(full_path);
        } else if (entry->d_type == DT_DIR || (entry->d_type == DT_UNKNOWN && is_directory(full_path))) {
            search_directory(full_path);
        }
    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    if (!is_directory(argv[1])) {
        fprintf(stderr, "Error: '%s' is not a directory\n", argv[1]);
        return 1;
    }

    srand((unsigned int)time(NULL));
    search_directory(argv[1]);

    return 0;
}
