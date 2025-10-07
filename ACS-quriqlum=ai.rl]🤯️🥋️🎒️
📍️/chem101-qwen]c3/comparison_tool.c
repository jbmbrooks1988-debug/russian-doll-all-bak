#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Comparison module for human vs AI learning speeds
// This module analyzes the progress of different students and compares learning rates

#define MAX_STUDENTS 10
#define MAX_QUIZ_SCORES 20
#define MAX_STUDENT_NAME_LEN 50

typedef struct {
    char name[MAX_STUDENT_NAME_LEN];
    float quiz_scores[MAX_QUIZ_SCORES];
    int num_scores;
    float learning_rate; // calculated rate of improvement
    int is_ai; // 1 if AI agent, 0 if human
} StudentPerformance;

// Function to calculate the learning rate based on quiz scores over time
float calculate_learning_rate(float* scores, int num_scores) {
    if (num_scores < 2) {
        return 0.0f;
    }
    
    // Calculate the slope of the learning curve using linear regression
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    
    for (int i = 0; i < num_scores; i++) {
        float x = (float)i; // time index
        float y = scores[i]; // score at that time
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    float n = (float)num_scores;
    float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    return slope;
}

// Function to add a new quiz score for a student
void add_quiz_score(StudentPerformance* student, float score) {
    if (student->num_scores < MAX_QUIZ_SCORES) {
        student->quiz_scores[student->num_scores] = score;
        student->num_scores++;
        
        // Recalculate learning rate
        student->learning_rate = calculate_learning_rate(student->quiz_scores, student->num_scores);
    }
}

// Function to load student performance data
int load_student_performance(const char* filepath, StudentPerformance* perf) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("Error: Could not open performance file %s\\n", filepath);
        return -1;
    }
    
    // For this demo, we'll simulate loading data
    // In a real implementation, we would parse the actual student data file
    strcpy(perf->name, "student_demo");
    perf->num_scores = 5;
    perf->quiz_scores[0] = 0.4f;
    perf->quiz_scores[1] = 0.5f;
    perf->quiz_scores[2] = 0.65f;
    perf->quiz_scores[3] = 0.72f;
    perf->quiz_scores[4] = 0.78f;
    perf->is_ai = 0;
    
    // Calculate initial learning rate
    perf->learning_rate = calculate_learning_rate(perf->quiz_scores, perf->num_scores);
    
    fclose(file);
    return 0;
}

// Function to compare learning performance between students
void compare_learning_performance(StudentPerformance* students, int num_students) {
    printf("Learning Performance Comparison\\n");
    printf("===============================\\n");
    printf("%-20s %-10s %-10s %-10s\\n", "Student", "Avg Score", "Learning Rate", "Type");
    printf("%-20s %-10s %-10s %-10s\\n", "-------", "---------", "-------------", "----");
    
    for (int i = 0; i < num_students; i++) {
        // Calculate average score
        float avg_score = 0.0f;
        for (int j = 0; j < students[i].num_scores; j++) {
            avg_score += students[i].quiz_scores[j];
        }
        if (students[i].num_scores > 0) {
            avg_score /= students[i].num_scores;
        }
        
        printf("%-20s %-9.3f  %-9.3f  %s\\n", 
               students[i].name, 
               avg_score, 
               students[i].learning_rate,
               students[i].is_ai ? "AI" : "Human");
    }
    
    printf("\\nAnalysis:\\n");
    printf("---------\\n");
    
    // Find the student with the highest learning rate
    int best_lr_idx = 0;
    for (int i = 1; i < num_students; i++) {
        if (students[i].learning_rate > students[best_lr_idx].learning_rate) {
            best_lr_idx = i;
        }
    }
    
    printf("Fastest learner: %s (Learning Rate: %.3f)\\n", 
           students[best_lr_idx].name, students[best_lr_idx].learning_rate);
    
    // Separate analysis for AI vs Human
    float ai_avg_rate = 0.0f, human_avg_rate = 0.0f;
    int ai_count = 0, human_count = 0;
    
    for (int i = 0; i < num_students; i++) {
        if (students[i].is_ai) {
            ai_avg_rate += students[i].learning_rate;
            ai_count++;
        } else {
            human_avg_rate += students[i].learning_rate;
            human_count++;
        }
    }
    
    if (ai_count > 0) ai_avg_rate /= ai_count;
    if (human_count > 0) human_avg_rate /= human_count;
    
    printf("Average AI Learning Rate: %.3f\\n", ai_avg_rate);
    printf("Average Human Learning Rate: %.3f\\n", human_avg_rate);
    
    if (ai_avg_rate > human_avg_rate) {
        printf("AI agents are learning faster on average.\\n");
    } else if (human_avg_rate > ai_avg_rate) {
        printf("Human students are learning faster on average.\\n");
    } else {
        printf("AI and human learning rates are equivalent.\\n");
    }
}

// Function to generate learning analytics report
void generate_learning_report(StudentPerformance* students, int num_students) {
    printf("\\nLearning Analytics Report\\n");
    printf("=========================\\n");
    
    for (int i = 0; i < num_students; i++) {
        printf("\\nStudent: %s (%s)\\n", students[i].name, students[i].is_ai ? "AI" : "Human");
        printf("Num quizzes: %d\\n", students[i].num_scores);
        
        if (students[i].num_scores > 0) {
            printf("Scores: ");
            for (int j = 0; j < students[i].num_scores; j++) {
                printf("%.2f ", students[i].quiz_scores[j]);
            }
            printf("\\n");
            printf("Learning Rate: %.3f\\n", students[i].learning_rate);
            
            // Identify improvement trend
            if (students[i].learning_rate > 0.05) {
                printf("Trend: Rapidly improving\\n");
            } else if (students[i].learning_rate > 0.01) {
                printf("Trend: Gradually improving\\n");
            } else if (students[i].learning_rate > -0.01) {
                printf("Trend: Plateauing\\n");
            } else {
                printf("Trend: Declining\\n");
            }
        }
    }
}

int main() {
    printf("Chemistry 101 Learning Speed Comparison Tool\\n");
    printf("============================================\\n");
    
    // Create sample student performance data
    StudentPerformance students[MAX_STUDENTS];
    int num_students = 3;
    
    // Student 1: Human
    strcpy(students[0].name, "student_001");
    students[0].num_scores = 7;
    students[0].quiz_scores[0] = 0.3f;  // Initial low score
    students[0].quiz_scores[1] = 0.4f;
    students[0].quiz_scores[2] = 0.5f;
    students[0].quiz_scores[3] = 0.55f;
    students[0].quiz_scores[4] = 0.62f;
    students[0].quiz_scores[5] = 0.68f;
    students[0].quiz_scores[6] = 0.72f;
    students[0].is_ai = 0;
    students[0].learning_rate = calculate_learning_rate(students[0].quiz_scores, students[0].num_scores);
    
    // Student 2: Human
    strcpy(students[1].name, "student_002");
    students[1].num_scores = 7;
    students[1].quiz_scores[0] = 0.45f;
    students[1].quiz_scores[1] = 0.52f;
    students[1].quiz_scores[2] = 0.58f;
    students[1].quiz_scores[3] = 0.64f;
    students[1].quiz_scores[4] = 0.69f;
    students[1].quiz_scores[5] = 0.71f;
    students[1].quiz_scores[6] = 0.75f;
    students[1].is_ai = 0;
    students[1].learning_rate = calculate_learning_rate(students[1].quiz_scores, students[1].num_scores);
    
    // Student 3: AI Agent
    strcpy(students[2].name, "ai_agent_001");
    students[2].num_scores = 7;
    students[2].quiz_scores[0] = 0.25f;  // AI might start lower
    students[2].quiz_scores[1] = 0.42f;
    students[2].quiz_scores[2] = 0.60f;
    students[2].quiz_scores[3] = 0.70f;
    students[2].quiz_scores[4] = 0.78f;
    students[2].quiz_scores[5] = 0.84f;
    students[2].quiz_scores[6] = 0.88f;
    students[2].is_ai = 1;
    students[2].learning_rate = calculate_learning_rate(students[2].quiz_scores, students[2].num_scores);
    
    // Compare performances
    compare_learning_performance(students, num_students);
    
    // Generate detailed report
    generate_learning_report(students, num_students);
    
    return 0;
}