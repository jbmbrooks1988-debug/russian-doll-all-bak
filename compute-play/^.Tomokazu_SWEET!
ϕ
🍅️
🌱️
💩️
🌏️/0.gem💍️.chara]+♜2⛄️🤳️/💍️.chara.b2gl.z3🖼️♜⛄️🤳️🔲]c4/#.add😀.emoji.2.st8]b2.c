#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

// ✅ Curated list of popular, colorful, widely supported emojis
const char* good_emojis[] = {
    // Faces
    "😀", "😃", "😄", "😁", "😆", "😅", "😂", "🤣", "😊", "😇",
    "🙂", "🙃", "😉", "😌", "😍", "🥰", "😘", "😗", "😙", "😚",
    "😋", "😛", "😝", "😜", "🤪", "🤨", "🧐", "🤓", "😎", "🤩",
    "🥳", "😏", "😒", "😞", "😔", "😟", "😕", "🙁", "☹️", "😣",
    "😖", "😫", "😩", "🥺", "😢", "😭", "😮", "😱", "😨", "😰",
    "😤", "😡", "😠", "🤬", "🤯", "😳", "🥵", "🥶", "😱", "😭",

    // Activities & Objects
    "🍕", "🍔", "🍟", "🌮", "🌯", "🍦", "🍩", "🍪", "🍫", "🧁",
    "🎮", "🕹️", "🎲", "🎯", "🎰", "🏈", "🏀", "⚽", "⚾", "🎾",
    "🎯", "🎳", "⛳", "🎣", "🎽", "🎿", "🏂", "🏋️", "🚴", "🏁",

    // Travel & Places
    "🚗", "🚕", "🚙", "🚌", "🚎", "🏎️", "🚓", "🚑", "🚒", "🚐",
    "🚀", "✈️", "🚁", "⛵", "🛸", "🚟", "🚇", "🚉", "🚀", "🛸",

    // Nature
    "🐶", "🐱", "🐭", "🐹", "🐰", "🦊", "🐻", "🐼", "🦁", "🐯",
    "🐨", "🐮", "🐷", "🐸", "🐵", "🐔", "🐧", "🐦", "🦆", "🦉",
    "🍎", "🍓", "🍒", "🍉", "🍇", "🍌", "🍍", "🥭", "🥑", "🥦",
    "🌻", "🌹", "🌸", "🌺", "🍀", "🌴", "🌵", "🌾", "🌿", "🍁",

    // Symbols & Fun
    "🌟", "⭐", "✨", "💥", "🔥", "🌈", "☀️", "🌤️", "⛅", "🌥️",
    "🌦️", "🌧️", "⛈️", "🌩️", "🌨️", "❄️", "☃️", "⛄", "🔥", "💧",
    "💯", "🎖️", "🏆", "🏅", "🥇", "🥈", "🥉", "🎉", "🎊", "🎈",
    "🎁", "🎄", "🎃", "🎭", "🎧", "🎵", "🎶", "🎤", "🎧", "🎨",
    "❤️", "🧡", "💛", "💚", "💙", "💜", "🖤", "🤍", "🤎", "💔"
};

#define NUM_GOOD_EMOJIS (sizeof(good_emojis) / sizeof(good_emojis[0]))

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

    // Pick random emoji from safe list
    const char* emoji = good_emojis[rand() % NUM_GOOD_EMOJIS];
    fprintf(file, "symbol : %s\n", emoji);
    fclose(file);

    printf("Updated: %s with emoji %s\n", filepath, emoji);
}

// Recursive search for state.txt
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
        fprintf(stderr, "Error: '%s' is not a valid directory\n", argv[1]);
        return 1;
    }

    srand((unsigned int)time(NULL));
    search_directory(argv[1]);

    return 0;
}
