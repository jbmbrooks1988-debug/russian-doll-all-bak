#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_ENTRIES 200
#define MAX_LINE 256
#define START_POINTS 3
#define WIN_POINTS 20

typedef struct {
    char emoji[16];
    char character[16];
    char english[64];
} Radical;

Radical radicals[MAX_ENTRIES];
int num_radicals = 0;

// Function to trim whitespace
char* trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Load radicals from the .txt file
int load_radicals(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return 0;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
        // Skip lines that are just numbers + emojis (section headers)
        char *trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;
        if (strstr(trimmed, "️") && (isdigit(trimmed[0]) || strstr(trimmed, "️"))) continue; // Skip 1️⃣️3️⃣️ etc.

        // Parse line: "emoji char Name Pinyin"
        char emoji[16] = {0}, character[16] = {0}, temp[128] = {0};
        if (sscanf(trimmed, "%15[^ ] %15[^ ] %127[^\n]", emoji, character, temp) == 3) {
            // Extract English name (ignore pinyin at end)
            char *last_space = strrchr(temp, ' ');
            if (last_space) {
                *last_space = '\0'; // Remove pinyin
            }
            strcpy(radicals[num_radicals].emoji, emoji);
            strcpy(radicals[num_radicals].character, character);
            strcpy(radicals[num_radicals].english, trim(temp));
            num_radicals++;
        }
    }
    fclose(file);
    return num_radicals > 0;
}

// Random number between 0 and n-1
int rand_range(int n) {
    return rand() % n;
}

// Check if two strings are equal (case insensitive for English)
int streq(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

// Display question and 4 choices
void quiz_round(int *score) {
    int correct_index = rand_range(num_radicals);
    Radical *correct = &radicals[correct_index];

    // Decide mode: 0 = show char, guess english; 1 = show english, guess char; 2 = show emoji, guess english
    int mode = rand_range(3);

    char question[64];
    char answer_pool[4][64];
    int choice_indices[4];
    choice_indices[0] = correct_index;

    // Fill other 3 random choices
    for (int i = 1; i < 4; i++) {
        do {
            choice_indices[i] = rand_range(num_radicals);
        } while (choice_indices[i] == choice_indices[i-1]);
    }

    // Shuffle indices
    for (int i = 0; i < 4; i++) {
        int j = rand_range(4);
        int tmp = choice_indices[i];
        choice_indices[i] = choice_indices[j];
        choice_indices[j] = tmp;
    }

    // Prepare question and answers based on mode
    switch (mode) {
        case 0: // Show character, guess English
            sprintf(question, "Character: '%s' means?", correct->character);
            for (int i = 0; i < 4; i++) {
                strcpy(answer_pool[i], radicals[choice_indices[i]].english);
            }
            break;
        case 1: // Show English, guess character
            sprintf(question, "What character means '%s'?", correct->english);
            for (int i = 0; i < 4; i++) {
                strcpy(answer_pool[i], radicals[choice_indices[i]].character);
            }
            break;
        case 2: // Show emoji, guess English
            sprintf(question, "Emoji: %s means?", correct->emoji);
            for (int i = 0; i < 4; i++) {
                strcpy(answer_pool[i], radicals[choice_indices[i]].english);
            }
            break;
    }

    // Display question
    printf("\n🎯 %s\n", question);
    for (int i = 0; i < 4; i++) {
        printf("  %d) %s\n", i+1, answer_pool[i]);
    }

    // Get user input
    int choice;
    printf("Enter your choice (1-4): ");
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n'); // clear input
        printf("Invalid input! ❌\n");
        return;
    }
    while (getchar() != '\n'); // clear input buffer

    if (choice < 1 || choice > 4) {
        printf("Choice out of range! ❌\n");
        return;
    }

    // Check answer
    int selected_index = -1;
    for (int i = 0; i < num_radicals; i++) {
        int match = 0;
        switch (mode) {
            case 0:
            case 2:
                match = streq(answer_pool[choice-1], radicals[i].english);
                break;
            case 1:
                match = strcmp(answer_pool[choice-1], radicals[i].character) == 0;
                break;
        }
        if (match) {
            selected_index = i;
            break;
        }
    }

    if (selected_index == correct_index) {
        printf("✅ Correct! +1 point\n");
        (*score)++;
    } else {
        printf("❌ Wrong! Correct was '%s'\n", 
               mode == 1 ? correct->character : correct->english);
        (*score)--;
    }

    printf("Current score: %d\n", *score);
}

int main() {
    srand(time(NULL));

    printf("🧩 Chinese Radical & Character Quiz Game 🧩\n");
    printf("Rules:\n");
    printf("• Start with %d points\n", START_POINTS);
    printf("• Correct: +1, Wrong: -1\n");
    printf("• Reach %d to win! Drop to 0 = Game Over\n\n", WIN_POINTS);

    if (!load_radicals("../ch.radical_legend🔢️]F6=♎️ ]a1.txt")) {
        printf("Failed to load data. Make sure the file is in the same directory.\n");
        return 1;
    }

    printf("Loaded %d radicals! 🎉\n", num_radicals);

    int score = START_POINTS;

    while (score > 0 && score < WIN_POINTS) {
        quiz_round(&score);
    }

    if (score >= WIN_POINTS) {
        printf("\n🏆 Congratulations! You Win! 🏆\n");
    } else {
        printf("\n💥 Game Over! You ran out of points.\n");
    }

    return 0;
}
