#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_QUESTIONS 50
#define LINE_LEN 512
#define FIELD_LEN 256

// Structure to hold a single Jeopardy clue
typedef struct {
    char category[FIELD_LEN];
    char question[FIELD_LEN];
    char answer[FIELD_LEN];
    int price;
    int used;  // To mark if this question has been used
} Clue;

Clue clues[MAX_QUESTIONS];
int num_clues = 0;

// Helper: Trim whitespace from a string
char* trim(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Load clues from CSV file
int load_clues(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Cannot open CSV file");
        return 0;
    }

    char line[LINE_LEN];
    // Skip header line
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Empty file or no header.\n");
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file) && num_clues < MAX_QUESTIONS) {
        char temp_line[LINE_LEN];
        strcpy(temp_line, line);

        char* fields[4];
        int i = 0;
        char* token = strtok(temp_line, ",");

        while (token && i < 4) {
            fields[i++] = token;
        }

        if (i != 4) {
            fprintf(stderr, "Invalid line (skipped): %s\n", line);
            continue;
        }

        Clue* c = &clues[num_clues];
        strcpy(c->category, trim(fields[0]));
        strcpy(c->question, trim(fields[1]));
        strcpy(c->answer, trim(fields[2]));
        c->price = atoi(trim(fields[3]));
        c->used = 0;

        num_clues++;
    }

    fclose(file);
    return 1;
}

// Show available categories and values
void show_board() {
    printf("\n--- JEOPARDY BOARD ---\n");
    // Group by category
    for (int i = 0; i < num_clues; i++) {
        if (i == 0 || strcmp(clues[i].category, clues[i-1].category) != 0) {
            printf("%s:\n", clues[i].category);
        }
        printf("  $%d", clues[i].price);
        if (clues[i].used) {
            printf(" [X]");
        }
        printf("\n");
    }
    printf("\n");
}

// Find a clue by category and price
Clue* find_clue(const char* category, int price) {
    for (int i = 0; i < num_clues; i++) {
        if (!clues[i].used &&
            strcasecmp(clues[i].category, category) == 0 &&
            clues[i].price == price) {
            return &clues[i];
        }
    }
    return NULL;
}

// Normalize answer: lowercase and trim
int compare_answers(const char* user, const char* correct) {
    char u[FIELD_LEN], c[FIELD_LEN];
    strcpy(u, user);
    strcpy(c, correct);

    // Convert to lowercase
    for (int i = 0; u[i]; i++) u[i] = tolower(u[i]);
    for (int i = 0; c[i]; i++) c[i] = tolower(c[i]);

    // Trim
    trim(u);
    trim(c);

    return strcmp(u, c) == 0;
}

// Main game loop
int main() {
    if (!load_clues("questions.csv")) {
        fprintf(stderr, "Failed to load clues.\n");
        return 1;
    }

    if (num_clues == 0) {
        fprintf(stderr, "No clues loaded.\n");
        return 1;
    }

    char input[FIELD_LEN];
    int score = 0;

    printf("Welcome to CLI Jeopardy!\n");

    while (1) {
        show_board();

        // Check if all clues are used
        int all_used = 1;
        for (int i = 0; i < num_clues; i++) {
            if (!clues[i].used) {
                all_used = 0;
                break;
            }
        }
        if (all_used) {
            printf("Game over! Final score: $%d\n", score);
            break;
        }

        printf("Enter category (or 'quit' to exit): ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        if (strcasecmp(input, "quit") == 0) break;

        char category[FIELD_LEN];
        strcpy(category, input);

        printf("Enter price (e.g., 100, 200): ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        int price = atoi(input);

        Clue* clue = find_clue(category, price);
        if (!clue) {
            printf("Invalid selection or already used. Try again.\n");
            continue;
        }

        printf("\nQuestion: %s\n", clue->question);
        printf("Your answer: ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        if (compare_answers(input, clue->answer)) {
            printf("Correct! You win $%d!\n", clue->price);
            score += clue->price;
        } else {
            printf("Incorrect! The correct answer was: %s\n", clue->answer);
            // Optionally deduct points? Or just no gain.
        }

        clue->used = 1;
        printf("Current score: $%d\n", score);
    }

    printf("Thanks for playing! Final score: $%d\n", score);
    return 0;
}
