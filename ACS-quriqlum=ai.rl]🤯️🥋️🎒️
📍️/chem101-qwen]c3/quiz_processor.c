#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VOCAB 100
#define MAX_QUIZ_QUESTIONS 50
#define MAX_WORD_LEN 100

// Vocabulary structure following the specified model
typedef struct {
    int number;
    char word[MAX_WORD_LEN];
    float embedding[2]; // embedding pe
    float weight;
    float bias[4]; // bias1, bias2, bias3, bias4
} VocabItem;

// Quiz question structure
typedef struct {
    int question_num;
    char prefix[MAX_WORD_LEN * 2];
    char suffix[MAX_WORD_LEN * 2];
    char predicted_answer[MAX_WORD_LEN];
} QuizQuestion;

// Vocabulary data array
VocabItem vocab_model[MAX_VOCAB];
int vocab_count = 0;

// Function to load the vocabulary model from file
int load_vocab_model(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open vocabulary file %s\n", filename);
        return -1;
    }

    char line[500];
    while (fgets(line, sizeof(line), file) && vocab_count < MAX_VOCAB) {
        // Skip comments and empty lines
        if (line[0] == '#' || strlen(line) < 10) continue;

        VocabItem* item = &vocab_model[vocab_count];
        char word[MAX_WORD_LEN];
        
        // Parse the line: number word embedding[2] pe weight bias[4]
        int fields = sscanf(line, "%d %s %f %f %f %f %f %f %f", 
            &item->number, word,
            &item->embedding[0], &item->embedding[1],
            &item->weight,
            &item->bias[0], &item->bias[1], &item->bias[2], &item->bias[3]);

        if (fields >= 5) {
            strcpy(item->word, word);
            vocab_count++;
        }
    }

    fclose(file);
    printf("Loaded %d vocabulary items\n", vocab_count);
    return 0;
}

// Function to calculate attention score for predicting the answer
float calculate_attention_score(const char* prefix, const char* suffix, const VocabItem* vocab) {
    // Simplified attention mechanism
    float prefix_score = 0.1f;
    float suffix_score = 0.1f;
    
    // Check if any part of the vocabulary word appears in prefix or suffix
    if (strstr(prefix, vocab->word) != NULL) {
        prefix_score = 1.0f;
    }
    
    if (strstr(suffix, vocab->word) != NULL) {
        suffix_score = 1.0f;
    }
    
    // Use the vocabulary weight
    float vocab_weight = vocab->weight;
    
    // Combined score
    return (prefix_score + suffix_score) * vocab_weight;
}

// Function to predict the best answer for a question
void predict_answer(const char* prefix, const char* suffix, char* predicted_answer) {
    float max_score = -1000.0f;
    int best_idx = 0;
    
    for (int i = 0; i < vocab_count; i++) {
        float score = calculate_attention_score(prefix, suffix, &vocab_model[i]);
        if (score > max_score) {
            max_score = score;
            best_idx = i;
        }
    }
    
    strcpy(predicted_answer, vocab_model[best_idx].word);
}

// Function to parse a quiz file and extract questions
int parse_quiz_file(const char* filename, QuizQuestion* questions) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open quiz file %s\n", filename);
        return -1;
    }

    char line[500];
    int question_count = 0;
    
    while (fgets(line, sizeof(line), file) && question_count < MAX_QUIZ_QUESTIONS) {
        // Look for lines with format "X. Text _____ text"
        if (strstr(line, "_____") != NULL) {
            // Extract question number
            int q_num;
            if (sscanf(line, "%d.", &q_num) == 1) {
                QuizQuestion* q = &questions[question_count];
                q->question_num = q_num;
                
                // Extract prefix and suffix around the blank
                char* blank_pos = strstr(line, "_____");
                if (blank_pos) {
                    // Copy prefix
                    int prefix_len = blank_pos - line;
                    strncpy(q->prefix, line, prefix_len);
                    q->prefix[prefix_len] = '\0';
                    
                    // Find end of blank and copy suffix
                    char* suffix_start = blank_pos + 5; // skip "_____"
                    strcpy(q->suffix, suffix_start);
                    
                    // Remove any trailing newline
                    char* newline = strchr(q->prefix, '\n');
                    if (newline) *newline = '\0';
                    newline = strchr(q->suffix, '\n');
                    if (newline) *newline = '\0';
                    
                    // Remove leading spaces from suffix
                    char *temp = q->suffix;
                    while (*temp == ' ') temp++;
                    memmove(q->suffix, temp, strlen(temp) + 1);
                    
                    // Predict the answer
                    predict_answer(q->prefix, q->suffix, q->predicted_answer);
                    
                    question_count++;
                }
            }
        }
    }

    fclose(file);
    return question_count;
}

// Function to process and display quiz results
void process_quiz(const char* quiz_filename) {
    QuizQuestion questions[MAX_QUIZ_QUESTIONS];
    int question_count = parse_quiz_file(quiz_filename, questions);
    
    if (question_count <= 0) {
        printf("No questions found in %s\\n", quiz_filename);
        return;
    }
    
    printf("\\nQuiz Processing Results for: %s\\n", quiz_filename);
    printf("================================================\\n");
    
    for (int i = 0; i < question_count; i++) {
        printf("Q%d: %s _____ %s\\n", 
            questions[i].question_num,
            questions[i].prefix,
            questions[i].suffix);
        printf("  Predicted Answer: %s\\n", questions[i].predicted_answer);
        printf("  -------------------------------------\\n");
    }
}

int main() {
    printf("Chemistry 101 Quiz Processing System\\n");
    printf("=====================================\\n");
    
    // Load vocabulary model
    if (load_vocab_model("chem101-curriculum/vocab_model.txt") != 0) {
        return 1;
    }
    
    // Process sample quizzes
    process_quiz("chem101-curriculum/chapter_001_atoms/mini_quizzes/quiz_001.txt");
    process_quiz("chem101-curriculum/chapter_002_molecules/mini_quizzes/quiz_001.txt");
    process_quiz("chem101-curriculum/chapter_003_reactions/mini_quizzes/quiz_001.txt");
    
    return 0;
}