#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Additional component for attention mechanism and AI agent
// This file implements the attention mechanism for better answer prediction

#define MAX_VOCAB 100
#define MAX_WORD_LEN 100

// Vocabulary structure following the specified model
typedef struct {
    int number;
    char word[MAX_WORD_LEN];
    float embedding[2]; // embedding pe
    float weight;
    float bias[4]; // bias1, bias2, bias3, bias4
    float score; // RL score
} VocabItem;

// Attention weights structure
typedef struct {
    float context_weights[MAX_VOCAB];
    float query_weights[MAX_VOCAB];
    float value_weights[MAX_VOCAB];
} AttentionWeights;

// Initialize attention mechanism weights
void init_attention_weights(AttentionWeights* attn, int vocab_size) {
    for (int i = 0; i < vocab_size; i++) {
        // Initialize with small random values
        attn->context_weights[i] = 0.1f * ((float)rand() / RAND_MAX);
        attn->query_weights[i] = 0.1f * ((float)rand() / RAND_MAX);
        attn->value_weights[i] = 0.1f * ((float)rand() / RAND_MAX);
    }
}

// Calculate attention scores using word embeddings and context
float calculate_attention_score_enhanced(const char* prefix, const char* suffix, 
                                        VocabItem* vocab, int vocab_count, 
                                        AttentionWeights* attn) {
    float total_score = 0.0f;
    
    // Process prefix and suffix to find matching vocabulary terms
    for (int i = 0; i < vocab_count; i++) {
        float prefix_match = 0.0f;
        float suffix_match = 0.0f;
        
        // Check for partial matches in prefix and suffix
        if (strstr(prefix, vocab[i].word) != NULL) {
            prefix_match = 1.0f;
        } else {
            // Check if any part of the word appears (substring match)
            for (int j = 0; j < strlen(vocab[i].word) - 2; j++) {
                char subword[MAX_WORD_LEN];
                strncpy(subword, vocab[i].word + j, 3);
                subword[3] = '\0';
                if (strstr(prefix, subword) != NULL) {
                    prefix_match = 0.3f;
                    break;
                }
            }
        }
        
        if (strstr(suffix, vocab[i].word) != NULL) {
            suffix_match = 1.0f;
        } else {
            // Check if any part of the word appears (substring match)
            for (int j = 0; j < strlen(vocab[i].word) - 2; j++) {
                char subword[MAX_WORD_LEN];
                strncpy(subword, vocab[i].word + j, 3);
                subword[3] = '\0';
                if (strstr(suffix, subword) != NULL) {
                    suffix_match = 0.3f;
                    break;
                }
            }
        }
        
        // Calculate attention using learned weights
        float attention = (prefix_match * attn->query_weights[i] + 
                          suffix_match * attn->context_weights[i]) * 
                         vocab[i].weight * (1.0f + vocab[i].score);
        
        // Add value component
        attention += attn->value_weights[i] * sqrt(vocab[i].embedding[0]*vocab[i].embedding[0] + 
                                                  vocab[i].embedding[1]*vocab[i].embedding[1]);
        
        total_score += attention;
    }
    
    return total_score;
}

// Enhanced prediction function using attention mechanism
void predict_answer_with_attention(const char* prefix, const char* suffix, 
                                   VocabItem* vocab, int vocab_count, 
                                   AttentionWeights* attn, char* predicted_answer) {
    float max_score = -10000.0f;
    int best_idx = 0;
    
    for (int i = 0; i < vocab_count; i++) {
        // Calculate attention score for this vocabulary item
        float score = calculate_attention_score_enhanced(prefix, suffix, &vocab[i], vocab_count, attn);
        
        if (score > max_score) {
            max_score = score;
            best_idx = i;
        }
    }
    
    strcpy(predicted_answer, vocab[best_idx].word);
}

// Function to update attention weights based on correct answers (learning)
void update_attention_weights(const char* prefix, const char* suffix, 
                              const char* correct_answer,
                              VocabItem* vocab, int vocab_count, 
                              AttentionWeights* attn) {
    for (int i = 0; i < vocab_count; i++) {
        if (strcmp(vocab[i].word, correct_answer) == 0) {
            // Increase weights for correct answer
            attn->query_weights[i] += 0.05f;
            attn->context_weights[i] += 0.05f;
            attn->value_weights[i] += 0.05f;
            
            // Normalize weights to prevent them from growing too large
            if (attn->query_weights[i] > 1.0f) attn->query_weights[i] = 1.0f;
            if (attn->context_weights[i] > 1.0f) attn->context_weights[i] = 1.0f;
            if (attn->value_weights[i] > 1.0f) attn->value_weights[i] = 1.0f;
        } else {
            // Slightly decrease weights for incorrect options
            attn->query_weights[i] -= 0.01f;
            attn->context_weights[i] -= 0.01f;
            attn->value_weights[i] -= 0.01f;
            
            // Ensure weights don't go negative
            if (attn->query_weights[i] < 0.0f) attn->query_weights[i] = 0.0f;
            if (attn->context_weights[i] < 0.0f) attn->context_weights[i] = 0.0f;
            if (attn->value_weights[i] < 0.0f) attn->value_weights[i] = 0.0f;
        }
    }
}

// Example usage function
void demonstrate_attention_mechanism() {
    printf("Demonstrating Enhanced Attention Mechanism\n");
    printf("==========================================\n");
    
    // Sample vocabulary items
    VocabItem sample_vocab[MAX_VOCAB];
    int vocab_count = 5;
    
    // Initialize sample vocabulary
    sample_vocab[0] = (VocabItem){1, "proton", {-0.8f, 0.5f}, 0.3f, {0.1f, 0.2f, -0.1f, 0.4f}, 0.0f};
    sample_vocab[1] = (VocabItem){2, "neutron", {0.2f, -0.3f}, 0.7f, {0.4f, -0.2f, 0.3f, 0.1f}, 0.0f};
    sample_vocab[2] = (VocabItem){3, "electron", {0.9f, 0.6f}, -0.4f, {0.2f, 0.5f, -0.3f, 0.6f}, 0.0f};
    sample_vocab[3] = (VocabItem){4, "atom", {-0.1f, 0.8f}, 0.2f, {0.7f, -0.4f, 0.1f, 0.3f}, 0.0f};
    sample_vocab[4] = (VocabItem){5, "nucleus", {0.4f, -0.2f}, 0.9f, {0.5f, 0.6f, -0.2f, 0.7f}, 0.0f};
    
    // Initialize attention weights
    AttentionWeights attn;
    init_attention_weights(&attn, vocab_count);
    
    // Test prediction
    const char* prefix = "The subatomic particle with a positive charge in the";
    const char* suffix = "is called the";
    char predicted_answer[MAX_WORD_LEN];
    
    predict_answer_with_attention(prefix, suffix, sample_vocab, vocab_count, &attn, predicted_answer);
    
    printf("Question: %s _____ %s\n", prefix, suffix);
    printf("Predicted answer: %s\n", predicted_answer);
    
    // Simulate learning with correct answer
    const char* correct_answer = "nucleus";
    printf("Correct answer: %s\n", correct_answer);
    
    update_attention_weights(prefix, suffix, correct_answer, sample_vocab, vocab_count, &attn);
    printf("Attention weights updated based on correct answer.\n\n");
    
    // Predict again to see improvement
    predict_answer_with_attention(prefix, suffix, sample_vocab, vocab_count, &attn, predicted_answer);
    printf("After learning, predicted answer: %s\n", predicted_answer);
}

int main() {
    // Demonstrate the attention mechanism
    demonstrate_attention_mechanism();
    
    return 0;
}