#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_QUESTIONS 100
#define LINE_LEN 512
#define FIELD_LEN 256

// ANSI Color & Sound Codes
#define RESET   "\x1b[0m"
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define BOLD    "\x1b[1m"
#define BEEP    "\x07"  // Terminal bell

// Structure for a Jeopardy clue
typedef struct {
    char category[FIELD_LEN];
    char question[FIELD_LEN];
    char answer[FIELD_LEN];
    int price;
    int used;
} Clue;

Clue clues[MAX_QUESTIONS];
int num_clues = 0;

// Trim whitespace (including \r, \n)
char* trim(char* str) {
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')) start++;
    if (*start == 0) {
        *str = 0;
        return str;
    }
    char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    end[1] = '\0';
    memmove(str, start, strlen(start) + 1);
    return str;
}

// Load clues from CSV
int load_clues(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Cannot open CSV file");
        return 0;
    }

    char line[LINE_LEN];

    // Skip header
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Error: Missing header.\n");
        fclose(file);
        return 0;
    }
    printf("📁 Debug: Header = '%s'", trim(line));

    while (fgets(line, sizeof(line), file) && num_clues < MAX_QUESTIONS) {
        line[strcspn(line, "\r\n")] = 0;  // Strip newline
        if (strlen(line) == 0) continue;

        char temp[LINE_LEN];
        strcpy(temp, line);

        char* fields[4];
        int idx = 0;
        char* token = strtok(temp, ",");

        while (token && idx < 4) {
            fields[idx++] = token;
            token = strtok(NULL, ",");
        }

        if (idx != 4) {
            fprintf(stderr, "❌ Bad line (fields: %d): '%s'\n", idx, line);
            continue;
        }

        Clue* c = &clues[num_clues];
        strcpy(c->category, trim(fields[0]));
        strcpy(c->question, trim(fields[1]));
        strcpy(c->answer, trim(fields[2]));
        c->price = atoi(trim(fields[3]));
        c->used = 0;

        if (c->price == 0 && strcmp(trim(fields[3]), "0") != 0) {
            fprintf(stderr, "⚠️ Invalid price: '%s'\n", fields[3]);
            continue;
        }

        printf("✅ Loaded: [%s] $%d → '%s'\n", c->category, c->price, c->question);
        num_clues++;
    }

    fclose(file);
    printf(GREEN "🎉 Total clues loaded: %d\n" RESET, num_clues);
    return num_clues > 0;
}

// Show the board
void show_board() {
    printf("\n" BOLD CYAN "--- JEOPARDY BOARD ---" RESET "\n");
    char current_category[FIELD_LEN] = "";

    for (int i = 0; i < num_clues; i++) {
        if (strcmp(clues[i].category, current_category) != 0) {
            strcpy(current_category, clues[i].category);
            printf(BOLD "%s:\n" RESET, current_category);
        }
        if (clues[i].used) {
            printf("  $%d " MAGENTA "[X]" RESET "\n", clues[i].price);
        } else {
            printf("  " YELLOW "$%d" RESET "\n", clues[i].price);
        }
    }
    printf(RESET "\n");
}

// Find clue by category and price
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

// Compare answers (case-insensitive, trimmed)
int compare_answers(const char* user, const char* correct) {
    char u[FIELD_LEN] = {0};
    char c[FIELD_LEN] = {0};
    int j = 0;

    // Convert and clean user input
    j = 0;
    for (int i = 0; user[i] && j < FIELD_LEN - 1; i++) {
        char ch = tolower(user[i]);
        if (isalnum(ch) || ch == ' ') {
            u[j++] = ch;
        }
    }
    u[j] = '\0';

    // Clean extra spaces in u
    char temp[FIELD_LEN];
    strcpy(temp, u);
    char* word = strtok(temp, " ");
    u[0] = '\0';
    while (word) {
        if (u[0]) strcat(u, " ");
        strcat(u, word);
        word = strtok(NULL, " ");
    }

    // Clean correct answer
    j = 0;
    for (int i = 0; correct[i] && j < FIELD_LEN - 1; i++) {
        char ch = tolower(correct[i]);
        if (isalnum(ch) || ch == ' ') {
            c[j++] = ch;
        }
    }
    c[j] = '\0';

    strcpy(temp, c);
    word = strtok(temp, " ");
    c[0] = '\0';
    while (word) {
        if (c[0]) strcat(c, " ");
        strcat(c, word);
        word = strtok(NULL, " ");
    }

    return strcmp(u, c) == 0;
}

// Main game
int main() {
    printf(GREEN "👋 Welcome to " BOLD "CLI JEOPARDY!" RESET GREEN " Let's play!\n" RESET);
    printf("%s", BEEP);  // Start beep

    if (!load_clues("questions.csv")) {
        fprintf(stderr, RED "❌ Failed to load questions.\n" RESET);
        return 1;
    }

    if (num_clues == 0) {
        fprintf(stderr, RED "No clues loaded. Check your CSV.\n" RESET);
        return 1;
    }

    char input[FIELD_LEN];
    int score = 0;

    while (1) {
        show_board();

        // Check if all used
        int all_used = 1;
        for (int i = 0; i < num_clues; i++) {
            if (!clues[i].used) {
                all_used = 0;
                break;
            }
        }
        if (all_used) {
            printf("🏆 Game over! Final score: " YELLOW "$%d\n" RESET, score);
            printf("%s%s", BEEP, BEEP);
            break;
        }

        printf(BOLD "Enter category (or 'quit' to exit): " RESET);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        if (strcasecmp(input, "quit") == 0) break;

        char category[FIELD_LEN];
        strcpy(category, input);

        printf(BOLD "Enter price (e.g., 100, 200): " RESET);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        int price = atoi(input);

        Clue* clue = find_clue(category, price);
        if (!clue) {
            printf(RED "❌ Invalid selection or already used. Try again.\n" RESET);
            continue;
        }

        printf("\n" BOLD BLUE "Question: %s\n" RESET, clue->question);
        printf("%s", BEEP);  // Beep when question appears

        printf(GREEN "Your answer: " RESET);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        if (compare_answers(input, clue->answer)) {
            printf(BOLD GREEN "🎉 Correct! You win $%d!" RESET "%s\n", clue->price, BEEP);
            score += clue->price;
        } else {
            printf(RED "❌ Incorrect! " RESET "The correct answer was: " BOLD "%s\n" RESET, clue->answer);
            printf("%s", BEEP);
        }

        printf("💡 Current score: " YELLOW "$%d\n" RESET, score);
        clue->used = 1;
    }

    printf("🙏 Thanks for playing! Final score: " YELLOW "$%d\n" RESET, score);
    printf("%s%s", BEEP, BEEP);  // Victory beeps
    return 0;
}
