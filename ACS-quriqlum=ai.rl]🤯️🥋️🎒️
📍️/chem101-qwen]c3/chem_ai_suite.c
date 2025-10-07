#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_VOCAB 100
#define MAX_QUIZ_QUESTIONS 50
#define MAX_WORD_LEN 100
#define MAX_STUDENTS 10
#define MAX_STUDENT_NAME_LEN 50

// Vocabulary structure following the specified model
typedef struct {
    int number;
    char word[MAX_WORD_LEN];
    float embedding[2]; // embedding pe
    float weight;
    float bias[4]; // bias1, bias2, bias3, bias4
    float score; // RL score
} VocabItem;

// Student data structure
typedef struct {
    char name[MAX_STUDENT_NAME_LEN];
    VocabItem vocab[MAX_VOCAB];
    int vocab_count;
    float quiz_scores[7]; // scores for 3 chapter mini, 3 chapter final, 1 comprehensive
    int study_sessions;
    char last_accessed[20];
    char ai_algorithm[50]; // only for AI agents
} StudentData;

// Quiz question structure
typedef struct {
    int question_num;
    char prefix[MAX_WORD_LEN * 2];
    char suffix[MAX_WORD_LEN * 2];
    char correct_answer[MAX_WORD_LEN];
    char predicted_answer[MAX_WORD_LEN];
    float confidence; // confidence in predicted answer
} QuizQuestion;

// Global vocabulary data
VocabItem vocab_model[MAX_VOCAB];
int vocab_count = 0;

// Function to load the vocabulary model from file
int load_vocab_model(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open vocabulary file %s\\n", filename);
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
            item->score = 0.0f; // Initialize RL score
            vocab_count++;
        }
    }

    fclose(file);
    printf("Loaded %d vocabulary items\\n", vocab_count);
    return 0;
}

// Function to load a student's data
int load_student_data(const char* student_dir, StudentData* student) {
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/student_data.txt", student_dir);
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("Error: Could not open student data file %s\\n", filepath);
        return -1;
    }
    
    // Set student name from directory
    const char* name_start = strrchr(student_dir, '/');
    if (name_start) {
        strcpy(student->name, name_start + 1);
    } else {
        strcpy(student->name, student_dir);
    }
    
    // Initialize student data
    student->vocab_count = 0;
    for (int i = 0; i < 7; i++) {
        student->quiz_scores[i] = 0.0;
    }
    student->study_sessions = 0;
    strcpy(student->last_accessed, "2025-09-24");
    strcpy(student->ai_algorithm, "None");
    
    // For this demo, copy global vocab to student vocab
    for (int i = 0; i < vocab_count && i < MAX_VOCAB; i++) {
        student->vocab[i] = vocab_model[i];
    }
    student->vocab_count = vocab_count;
    
    fclose(file);
    return 0;
}

// Function to save a student's data
int save_student_data(const char* student_dir, StudentData* student) {
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "%s/student_data.txt", student_dir);
    
    FILE* file = fopen(filepath, "w");
    if (!file) {
        printf("Error: Could not write to student data file %s\\n", filepath);
        return -1;
    }
    
    fprintf(file, "# %s Memory File\\n", student->name);
    fprintf(file, "# Format: number word embedding pe weight bias1 bias2 bias3 bias4 score\\n\\n");
    
    // Save vocabulary with updated scores
    for (int i = 0; i < student->vocab_count; i++) {
        VocabItem* item = &student->vocab[i];
        fprintf(file, "%d %s %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\\n", 
            item->number, item->word,
            item->embedding[0], item->embedding[1],
            item->weight,
            item->bias[0], item->bias[1], item->bias[2], item->bias[3],
            item->score);
    }
    
    fprintf(file, "\\n# Quiz performance tracking\\n");
    fprintf(file, "quiz_scores: {\\n");
    fprintf(file, "  chapter_1_mini_1: %.3f,\\n", student->quiz_scores[0]);
    fprintf(file, "  chapter_1_final: %.3f,\\n", student->quiz_scores[1]);
    fprintf(file, "  chapter_2_mini_1: %.3f,\\n", student->quiz_scores[2]);
    fprintf(file, "  chapter_2_final: %.3f,\\n", student->quiz_scores[3]);
    fprintf(file, "  chapter_3_mini_1: %.3f,\\n", student->quiz_scores[4]);
    fprintf(file, "  chapter_3_final: %.3f,\\n", student->quiz_scores[5]);
    fprintf(file, "  comprehensive_final: %.3f\\n", student->quiz_scores[6]);
    fprintf(file, "}\\n\\n");
    
    fprintf(file, "# Learning progress\\n");
    fprintf(file, "last_accessed: \"%s\"\\n", student->last_accessed);
    fprintf(file, "study_sessions: %d\\n", student->study_sessions);
    
    if (strlen(student->ai_algorithm) > 0) {
        fprintf(file, "ai_algorithm: \"%s\"\\n", student->ai_algorithm);
    }
    
    fclose(file);
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
    
    // Use the vocabulary weight and accumulated RL score
    float vocab_weight = vocab->weight + vocab->score; // Incorporate RL score
    
    // Combined score
    return (prefix_score + suffix_score) * vocab_weight;
}

// Function to predict the best answer for a question
void predict_answer(const char* prefix, const char* suffix, StudentData* student, char* predicted_answer) {
    float max_score = -1000.0f;
    int best_idx = 0;
    
    for (int i = 0; i < student->vocab_count; i++) {
        float score = calculate_attention_score(prefix, suffix, &student->vocab[i]);
        if (score > max_score) {
            max_score = score;
            best_idx = i;
        }
    }
    
    strcpy(predicted_answer, student->vocab[best_idx].word);
}

// Function to evaluate quiz answers and update RL scores
void evaluate_quiz_answers(QuizQuestion* questions, int question_count, StudentData* student, int quiz_index) {
    // For this demo, we'll use a simple answer key
    char* answer_key[] = {
        // Chapter 1 mini quiz answers
        "electrons", "protons", "shells", "sublevels", "Aufbau", 
        "atomic", "orbitals", "spin", "2", "2p²",
        
        // Chapter 1 final test answers
        "positive", "neutral", "negative", "element", "protons", "neutrons",
        "lower", "higher", "l", "ml", "ms", "uncertainty",
        "s", "p", "nl^x", "principal quantum number", "sublevel", "number of electrons",
        "atomos", "Bohr", "waves", "wave", "particle", "2p",
        
        // Chapter 2 answers
        "ionic", "electron", "octet", "hydrogen", "London", "size",
        "share", "tetrahedral", "Hydrogen", "N", "ide", "di",
        
        // Chapter 2 final test answers
        "covalent", "metallic", "minimize", "least", "London",
        "hydrogen", "tetrahedral", "109.5", "Molecular", "Ionic",
        "oxidation", "sodium chloride", "ionic", "intermolecular",
        
        // Chapter 3 answers
        "single", "balanced", "double displacement", "rate", "activation", "energy",
        "equal", "products", "minimize", "equilibrium",
        
        // Chapter 3 final test answers
        "combustion", "mass", "AD", "CB", "orders",
        "catalyst", "same", "reactants", "solids", "liquids",
        "energy", "rate-determining",
        
        // Comprehensive final answers
        "proton", "protons", "2", "covalent", "electron", "nitrogen",
        "mass", "activation", "reverse", "coefficients", "level", "bent",
        "oxygen", "concentrations", "pressure"
    };
    
    int correct = 0;
    int total = question_count;
    
    for (int i = 0; i < question_count && i < sizeof(answer_key)/sizeof(answer_key[0]); i++) {
        strcpy(questions[i].correct_answer, answer_key[i]);
        
        if (strcmp(questions[i].predicted_answer, answer_key[i]) == 0) {
            correct++;
            // Update RL score for correct answer
            for (int j = 0; j < student->vocab_count; j++) {
                if (strcmp(student->vocab[j].word, answer_key[i]) == 0) {
                    student->vocab[j].score += 0.1f; // Reward correct answer
                    break;
                }
            }
        } else {
            // Penalize incorrect answers
            for (int j = 0; j < student->vocab_count; j++) {
                if (strcmp(student->vocab[j].word, questions[i].predicted_answer) == 0) {
                    student->vocab[j].score -= 0.05f; // Penalty for incorrect answer
                    break;
                }
            }
        }
    }
    
    float score = (float)correct / total;
    student->quiz_scores[quiz_index] = score;
    
    printf("Quiz Results: %d/%d correct (%.2f%%)\\n", correct, total, score * 100);
}

// Function to parse a quiz file and extract questions
int parse_quiz_file(const char* filename, QuizQuestion* questions) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open quiz file %s\\n", filename);
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
                    
                    // Initialize answer fields
                    strcpy(q->predicted_answer, "unknown");
                    strcpy(q->correct_answer, "");
                    q->confidence = 0.0f;
                    
                    question_count++;
                }
            }
        }
    }

    fclose(file);
    return question_count;
}

// Function to process and display quiz results
void process_quiz(const char* quiz_filename, StudentData* student, int quiz_index) {
    QuizQuestion questions[MAX_QUIZ_QUESTIONS];
    int question_count = parse_quiz_file(quiz_filename, questions);
    
    if (question_count <= 0) {
        printf("No questions found in %s\\n", quiz_filename);
        return;
    }
    
    // Predict answers for each question
    for (int i = 0; i < question_count; i++) {
        predict_answer(questions[i].prefix, questions[i].suffix, student, questions[i].predicted_answer);
    }
    
    printf("\\nQuiz Processing Results for: %s\\n", quiz_filename);
    printf("Student: %s\\n", student->name);
    printf("================================================\\n");
    
    for (int i = 0; i < question_count; i++) {
        printf("Q%d: %s _____ %s\\n", 
            questions[i].question_num,
            questions[i].prefix,
            questions[i].suffix);
        printf("  Predicted Answer: %s\\n", questions[i].predicted_answer);
        printf("  -------------------------------------\\n");
    }
    
    // Evaluate answers and update RL scores
    evaluate_quiz_answers(questions, question_count, student, quiz_index);
}

// Function to run quizzes for a student
void run_student_quizzes(StudentData* student, const char* student_dir) {
    printf("\\nProcessing quizzes for student: %s\\n", student->name);
    printf("===========================================\\n");
    
    // Process all the quizzes for this student
    process_quiz("chem101-curriculum/chapter_001_atoms/mini_quizzes/quiz_001.txt", student, 0); // chapter 1 mini 1
    process_quiz("chem101-curriculum/chapter_001_atoms/chapter_final_test.txt", student, 1); // chapter 1 final
    
    process_quiz("chem101-curriculum/chapter_002_molecules/mini_quizzes/quiz_001.txt", student, 2); // chapter 2 mini 1
    process_quiz("chem101-curriculum/chapter_002_molecules/chapter_final_test.txt", student, 3); // chapter 2 final
    
    process_quiz("chem101-curriculum/chapter_003_reactions/mini_quizzes/quiz_001.txt", student, 4); // chapter 3 mini 1
    process_quiz("chem101-curriculum/chapter_003_reactions/chapter_final_test.txt", student, 5); // chapter 3 final
    
    process_quiz("chem101-curriculum/comprehensive_final_exam.txt", student, 6); // comprehensive final
    
    // Update student metadata
    student->study_sessions++;
    time_t now = time(0);
    struct tm *tm_info = localtime(&now);
    strftime(student->last_accessed, sizeof(student->last_accessed), "%Y-%m-%d", tm_info);
    
    // Save updated student data
    save_student_data(student_dir, student);
    
    printf("\\nQuiz session completed for %s\\n", student->name);
    printf("Study sessions: %d\\n", student->study_sessions);
}

int main() {
    printf("Chemistry 101 AI Training Suite\\n");
    printf("===============================\\n");
    
    // Load vocabulary model
    if (load_vocab_model("chem101-curriculum/vocab_model.txt") != 0) {
        return 1;
    }
    
    // Define student directories
    const char* student_dirs[] = {
        "student♟️_mem/student♟️_001",
        "student♟️_mem/student♟️_002", 
        "student♟️_mem/student♟️_ai_001"
    };
    int num_students = sizeof(student_dirs) / sizeof(student_dirs[0]);
    
    // Process each student
    for (int i = 0; i < num_students; i++) {
        StudentData student;
        if (load_student_data(student_dirs[i], &student) == 0) {
            run_student_quizzes(&student, student_dirs[i]);
            printf("\n======================================================================\n");
        } else {
            printf("Failed to load student data for %s\\n", student_dirs[i]);
        }
    }
    
    printf("\\nAll students have completed their quiz sessions.\\n");
    printf("RL scores have been updated based on performance.\\n");
    
    return 0;
}