#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_QUESTIONS 100
#define LINE_LEN 512
#define FIELD_LEN 256
#define MAX_TOKENS 10
#define MAX_WORD 30

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

// Trim whitespace
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

// Levenshtein Distance (edit distance) between two strings
int levenshtein(const char* s1, const char* s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    int matrix[len1 + 1][len2 + 1];

    for (int i = 0; i <= len1; i++) matrix[i][0] = i;
    for (int j = 0; j <= len2; j++) matrix[0][j] = j;

    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (tolower(s1[i-1]) == tolower(s2[j-1])) ? 0 : 1;
            matrix[i][j] = matrix[i-1][j] + 1;
            matrix[i][j] = (matrix[i][j-1] + 1 < matrix[i][j]) ? matrix[i][j-1] + 1 : matrix[i][j];
            matrix[i][j] = (matrix[i-1][j-1] + cost < matrix[i][j]) ? matrix[i-1][j-1] + cost : matrix[i][j];
        }
    }
    return matrix[len1][len2];
}

// Is string close enough using Levenshtein?
int is_similar(const char* a, const char* b) {
    int len_a = strlen(a);
    int len_b = strlen(b);
    int max_len = (len_a > len_b) ? len_a : len_b;
    if (max_len == 0) return 0;
    int distance = levenshtein(a, b);
    double similarity = 1.0 - (double)distance / max_len;
    return similarity >= 0.75;  // 75% similar
}

// Clean and tokenize string into keywords
void extract_keywords(char* input, char tokens[MAX_TOKENS][MAX_WORD], int* count) {
    *count = 0;
    if (strlen(input) == 0) return;

    char clean[FIELD_LEN] = {0};
    int j = 0;
    for (int i = 0; input[i]; i++) {
        char ch = tolower(input[i]);
        if (isalnum(ch) || ch == ' ') {
            clean[j++] = ch;
        }
    }
    clean[j] = '\0';

    // Collapse spaces and tokenize
    char temp[FIELD_LEN];
    strcpy(temp, clean);
    char* word = strtok(temp, " ");
    while (word && *count < MAX_TOKENS) {
        strcpy(tokens[*count], word);
        (*count)++;
        word = strtok(NULL, " ");
    }
}

// Compare answers: keyword overlap + Levenshtein typo tolerance
int compare_answers(const char* user, const* correct) {
    // Step 1: Clean full strings and try exact match
    char u_clean[FIELD_LEN] = {0}, c_clean[FIELD_LEN] = {0};
    int j = 0;
    for (int i = 0; user[i] && j < FIELD_LEN - 1; i++) {
        char ch = tolower(user[i]);
        if (isalnum(ch) || ch == ' ') u_clean[j++] = ch;
    }
    u_clean[j] = '\0';

    j = 0;
    for (int i = 0; correct[i] && j < FIELD_LEN - 1; i++) {
        char ch = tolower(correct[i]);
        if (isalnum(ch) || ch == ' ') c_clean[j++] = ch;
    }
    c_clean[j] = '\0';

    if (strcmp(u_clean, c_clean) == 0) return 1;

    // Step 2: Try Levenshtein on full cleaned strings
    if (is_similar(u_clean, c_clean)) return 1;

    // Step 3: Tokenize both
    char u_tokens[MAX_TOKENS][MAX_WORD], c_tokens[MAX_TOKENS][MAX_WORD];
    int u_count = 0, c_count = 0;
    extract_keywords(user, u_tokens, &u_count);
    extract_keywords(correct, c_tokens, &c_count);

    if (u_count == 0 || c_count == 0) return 0;

    // Step 4: Count exact or fuzzy word matches
    int match_count = 0;
    for (int i = 0; i < u_count; i++) {
        for (int j = 0; j < c_count; j++) {
            if (strcmp(u_tokens[i], c_tokens[j]) == 0) {
                match_count++;
                break;
            }
            // Or close enough (e.g., "downy" ~ "downey")
            if (is_similar(u_tokens[i], c_tokens[j])) {
                match_count++;
                break;
            }
        }
    }

    // Require: at least one strong match, and 50%+ of user words match
    if (match_count > 0 && (double)match_count / u_count >= 0.5) return 1;

    // If correct answer is short (1-2 words), one fuzzy match is enough
    if (c_count <= 2 && match_count >= 1) return 1;

    return 0;
}

// Load clues from CSV
int load_clues(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Cannot open CSV file");
        return 0;
    }

    char line[LINE_LEN];
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Error: Missing header.\n");
        fclose(file);
        return 0;
    }
    printf("📁 Debug: Header = '%s'", trim(line));

    while (fgets(line, sizeof(line), file) && num_clues < MAX_QUESTIONS) {
        line[strcspn(line, "\r\n")] = 0;
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

// Main game
int main() {
    printf(GREEN "👋 Welcome to " BOLD "CLI JEOPARDY!" RESET GREEN " Let's play!\n" RESET);
    printf("%s", BEEP);

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
        printf("%s", BEEP);

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
    printf("%s%s", BEEP, BEEP);
    return 0;
}
