#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ROWS 100
#define MAX_COLS 5
#define MAX_LEN 256

// Struct for a compound
typedef struct {
    char name[MAX_LEN];
    char formula[MAX_LEN];
    char type[MAX_LEN];
    char func_groups[MAX_LEN];
    char relevance[MAX_LEN];
} Compound;

// Function to generate explanation for wrong answer
void explain_wrong_answer(Compound *c, int quiz_type, char *user_answer) {
    if (quiz_type == 1) {
        printf("Nope, it's %s.\n", c->type);
        printf("Explanation: The compound %s (%s) is a %s because:\n", c->name, c->formula, c->type);
        printf("- Functional Groups: %s define this type. Look for these in the formula.\n", c->func_groups);
        printf("- Nomenclature Hint: Names like %s often indicate %s (e.g., '-ol' for alcohols, '-ine' or '-ate' for amino acids).\n", c->name, c->type);
        printf("- MCAT Context: %s\n", c->relevance);
        printf("Tip: Focus on the functional groups (%s) and visualize the structure to classify it.\n\n", c->func_groups);
    } else {
        printf("Nope, it's %s.\n", c->func_groups);
        printf("Explanation: The compound %s (%s) has these functional groups:\n", c->name, c->formula);
        printf("- Groups: %s. For example, -NH2 is an amino group, -COOH is a carboxyl group.\n", c->func_groups);
        printf("- Structure Clue: In the formula (%s), identify %s to spot these groups.\n", c->formula, c->func_groups);
        printf("- MCAT Context: %s\n", c->relevance);
        printf("Tip: Break down the formula to find %s. Practice drawing the structure to see where they attach.\n\n", c->func_groups);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <csv_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        printf("Error opening %s\n", argv[1]);
        return 1;
    }

    Compound compounds[MAX_ROWS];
    int num_rows = 0;
    char line[MAX_LEN * MAX_COLS];
    char *token;

    // Skip header
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 1;
    }

    // Read rows
    while (fgets(line, sizeof(line), file) && num_rows < MAX_ROWS) {
        token = strtok(line, ",");
        if (token) strncpy(compounds[num_rows].name, token, MAX_LEN - 1);
        token = strtok(NULL, ",");
        if (token) strncpy(compounds[num_rows].formula, token, MAX_LEN - 1);
        token = strtok(NULL, ",");
        if (token) strncpy(compounds[num_rows].type, token, MAX_LEN - 1);
        token = strtok(NULL, ",");
        if (token) strncpy(compounds[num_rows].func_groups, token, MAX_LEN - 1);
        token = strtok(NULL, ",");
        if (token) strncpy(compounds[num_rows].relevance, token, MAX_LEN - 1);

        num_rows++;
    }
    fclose(file);

    if (num_rows == 0) {
        printf("No data found in CSV.\n");
        return 1;
    }

    srand(time(NULL));  // Seed random

    printf("MCAT Compound Quiz! (Press Enter to reveal answers)\n");
    printf("Type 'q' to quit.\n\n");

    while (1) {
        int idx = rand() % num_rows;
        Compound *c = &compounds[idx];

        printf("Compound: %s\n", c->name);
        printf("Formula: %s\n", c->formula);

        int quiz_type;
        printf("Quiz on: 1=Type, 2=Functional Groups? Enter 1 or 2: ");
        scanf("%d", &quiz_type);
        while (getchar() != '\n');  // Clear input

        char answer[MAX_LEN];
        if (quiz_type == 1) {
            printf("What type is it? ");
            fgets(answer, sizeof(answer), stdin);
            answer[strcspn(answer, "\n")] = 0;  // Remove newline
            if (strcmp(answer, c->type) == 0) {
                printf("Correct!\n");
            } else {
                explain_wrong_answer(c, quiz_type, answer);
            }
        } else {
            printf("What functional groups? ");
            fgets(answer, sizeof(answer), stdin);
            answer[strcspn(answer, "\n")] = 0;
            if (strcmp(answer, c->func_groups) == 0) {
                printf("Correct!\n");
            } else {
                explain_wrong_answer(c, quiz_type, answer);
            }
        }

        printf("Relevance: %s\n\n", c->relevance);

        printf("Next? (Enter 'q' to quit): ");
        fgets(answer, sizeof(answer), stdin);
        answer[strcspn(answer, "\n")] = 0;
        if (strcmp(answer, "q") == 0) break;
    }

    return 0;
}
