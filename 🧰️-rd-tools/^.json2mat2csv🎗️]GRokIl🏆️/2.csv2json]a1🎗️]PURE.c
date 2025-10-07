#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROWS 1000
#define MAX_COLS 2
#define MAX_CELL_SIZE 1024

// Function to skip whitespace
int skip_whitespace(const char* str, int pos) {
    while (pos >= 0 && str[pos] != '\0' && 
           (str[pos] == ' ' || str[pos] == '\n' || str[pos] == '\t' || str[pos] == '\r')) {
        pos++;
    }
    return pos;
}

// Function to read CSV file contents
char* read_csv_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, fp);
    buffer[read] = '\0';
    
    fclose(fp);
    return buffer;
}

// Function to parse CSV line into key-value pair
int parse_csv_line(const char* csv, int pos, char* key, char* value, int max_size) {
    int i = 0;
    
    // Parse key (assume quoted)
    pos++; // Skip opening quote
    while (csv[pos] != '"' && csv[pos] != '\0' && i < max_size - 1) {
        key[i++] = csv[pos++];
    }
    key[i] = '\0';
    if (csv[pos] != '"') return -1;
    pos += 2; // Skip closing quote and comma
    
    // Parse value (assume quoted)
    i = 0;
    pos++; // Skip opening quote
    while (csv[pos] != '"' && csv[pos] != '\0' && i < max_size - 1) {
        value[i++] = csv[pos++];
    }
    value[i] = '\0';
    if (csv[pos] != '"') return -1;
    pos++; // Skip closing quote
    
    return csv[pos] == '\n' || csv[pos] == '\0' ? pos + 1 : -1;
}

// Function to resize matrix if needed
int resize_matrix(char*** matrix, int* max_rows, int current_rows) {
    if (current_rows >= *max_rows) {
        *max_rows *= 2;
        char** temp = realloc(*matrix, *max_rows * sizeof(char*));
        if (!temp) return -1;
        *matrix = temp;
        for (int i = current_rows; i < *max_rows; i++) {
            (*matrix)[i] = calloc(MAX_COLS * MAX_CELL_SIZE, sizeof(char));
            if (!(*matrix)[i]) return -1;
        }
    }
    return 0;
}

// Function to determine if a string is a number
int is_number(const char* str) {
    int has_decimal = 0;
    int i = 0;
    if (str[i] == '-') i++;
    while (str[i]) {
        if (str[i] == '.' && !has_decimal) {
            has_decimal = 1;
        } else if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
        i++;
    }
    return 1;
}

// Function to parse CSV into matrix
int parse_csv_to_matrix(const char* csv, char*** matrix, int* row_count, int* max_rows) {
    int pos = 0;
    *row_count = 0;

    while (csv[pos] != '\0') {
        if (resize_matrix(matrix, max_rows, *row_count) < 0) return -1;

        char key[MAX_CELL_SIZE];
        char value[MAX_CELL_SIZE];
        
        pos = parse_csv_line(csv, pos, key, value, MAX_CELL_SIZE);
        if (pos < 0) return -1;

        strncpy((*matrix)[*row_count], key, MAX_CELL_SIZE - 1);
        strncpy((*matrix)[*row_count] + MAX_CELL_SIZE, value, MAX_CELL_SIZE - 1);
        (*row_count)++;
        
        pos = skip_whitespace(csv, pos);
    }
    return 0;
}

// Function to write JSON from matrix
int csv_to_json(const char* csv, const char* output_file) {
    int max_rows = MAX_ROWS;
    int row_count = 0;

    char** matrix = malloc(max_rows * sizeof(char*));
    if (!matrix) return -1;
    
    for (int i = 0; i < max_rows; i++) {
        matrix[i] = calloc(MAX_COLS * MAX_CELL_SIZE, sizeof(char));
        if (!matrix[i]) {
            for (int j = 0; j < i; j++) free(matrix[j]);
            free(matrix);
            return -1;
        }
    }

    if (parse_csv_to_matrix(csv, &matrix, &row_count, &max_rows) < 0) {
        for (int i = 0; i < max_rows; i++) free(matrix[i]);
        free(matrix);
        return -1;
    }

    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        for (int i = 0; i < max_rows; i++) free(matrix[i]);
        free(matrix);
        return -1;
    }

    fprintf(fp, "{\n");
    for (int i = 0; i < row_count; i++) {
        char* key = matrix[i];
        char* value = matrix[i] + MAX_CELL_SIZE;
        char* separator = (i < row_count - 1) ? "," : "";

        // Handle nested objects and arrays
        if (strchr(key, '.') || strchr(key, '[')) {
            // For simplicity, we'll just write flat key-value pairs
            if (is_number(value)) {
                fprintf(fp, "  \"%s\": %s%s\n", key, value, separator);
            } else {
                fprintf(fp, "  \"%s\": \"%s\"%s\n", key, value, separator);
            }
        } else {
            if (is_number(value)) {
                fprintf(fp, "  \"%s\": %s%s\n", key, value, separator);
            } else {
                fprintf(fp, "  \"%s\": \"%s\"%s\n", key, value, separator);
            }
        }
    }
    fprintf(fp, "}\n");

    fclose(fp);
    for (int i = 0; i < max_rows; i++) free(matrix[i]);
    free(matrix);
    return 0;
}

// Test program with file input
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <csv_file>\n", argv[0]);
        return 1;
    }

    char* csv = read_csv_file(argv[1]);
    if (!csv) {
        printf("Error: Could not read file '%s'\n", argv[1]);
        return 1;
    }

    if (csv_to_json(csv, "output.json") == 0) {
        printf("JSON file has been generated as 'output.json'\n");
    } else {
        printf("Error converting CSV to JSON\n");
    }

    free(csv);
    return 0;
}
