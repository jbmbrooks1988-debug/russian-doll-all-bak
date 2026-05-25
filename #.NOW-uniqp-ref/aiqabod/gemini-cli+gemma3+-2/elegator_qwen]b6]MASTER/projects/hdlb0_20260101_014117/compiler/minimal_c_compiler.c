// Minimal C Compiler for 16-bit RISC-V Processor
// A basic compiler that translates a subset of C to 16-bit RISC-V assembly

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LENGTH 32
#define MAX_CODE_SIZE 1024
#define MAX_VARIABLES 32

// Token types
typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_ASTERISK,
    TOKEN_SLASH,
    TOKEN_SEMICOLON,
    TOKEN_ASSIGN,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EOF
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LENGTH];
    int int_value;
} Token;

// Variable table entry
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    int reg_num;  // Register number assigned to this variable
    int used;
} Variable;

// Global variables
Token current_token;
char *source;
int source_pos;
Variable variables[MAX_VARIABLES];
int next_reg = 1;  // Start from register 1 (x1), x0 is always 0

// Function prototypes
void next_token();
void parse_program();
void parse_statement();
void parse_expression();
int parse_term();
int parse_factor();
void compile_to_asm();
void init_compiler(char *src);
void error(const char *msg);

// Initialize the compiler with source code
void init_compiler(char *src) {
    source = src;
    source_pos = 0;
    next_token();
    
    // Initialize variable table
    for (int i = 0; i < MAX_VARIABLES; i++) {
        variables[i].used = 0;
    }
}

// Get the next character from source
char next_char() {
    if (source[source_pos] == '\0') {
        return '\0';
    }
    return source[source_pos++];
}

// Skip whitespace
void skip_whitespace() {
    char c = source[source_pos];
    while (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        c = next_char();
    }
}

// Look at the next character without consuming it
char peek_char() {
    if (source[source_pos] == '\0') {
        return '\0';
    }
    return source[source_pos];
}

// Get the next token
void next_token() {
    skip_whitespace();
    
    char c = next_char();
    
    switch (c) {
        case '\0':
            current_token.type = TOKEN_EOF;
            strcpy(current_token.value, "EOF");
            break;
            
        case '+':
            current_token.type = TOKEN_PLUS;
            strcpy(current_token.value, "+");
            break;
            
        case '-':
            current_token.type = TOKEN_MINUS;
            strcpy(current_token.value, "-");
            break;
            
        case '*':
            current_token.type = TOKEN_ASTERISK;
            strcpy(current_token.value, "*");
            break;
            
        case '/':
            current_token.type = TOKEN_SLASH;
            strcpy(current_token.value, "/");
            break;
            
        case ';':
            current_token.type = TOKEN_SEMICOLON;
            strcpy(current_token.value, ";");
            break;
            
        case '=':
            current_token.type = TOKEN_ASSIGN;
            strcpy(current_token.value, "=");
            break;
            
        case '(':
            current_token.type = TOKEN_LPAREN;
            strcpy(current_token.value, "(");
            break;
            
        case ')':
            current_token.type = TOKEN_RPAREN;
            strcpy(current_token.value, ")");
            break;
            
        case '0'...'9':
            // Number token
            current_token.type = TOKEN_NUMBER;
            current_token.value[0] = c;
            int i = 1;
            
            while (isdigit(peek_char())) {
                current_token.value[i++] = next_char();
            }
            current_token.value[i] = '\0';
            current_token.int_value = atoi(current_token.value);
            break;
            
        case 'a'...'z':
        case 'A'...'Z':
        case '_':
            // Identifier token
            current_token.type = TOKEN_IDENTIFIER;
            current_token.value[0] = c;
            int i = 1;
            
            while (isalnum(peek_char()) || peek_char() == '_') {
                current_token.value[i++] = next_char();
            }
            current_token.value[i] = '\0';
            break;
            
        default:
            error("Unexpected character");
    }
}

// Parse the entire program
void parse_program() {
    while (current_token.type != TOKEN_EOF) {
        parse_statement();
    }
}

// Parse a statement (currently only assignment statements)
void parse_statement() {
    // Expect an identifier (variable name)
    if (current_token.type != TOKEN_IDENTIFIER) {
        error("Expected identifier");
    }
    
    char var_name[MAX_TOKEN_LENGTH];
    strcpy(var_name, current_token.value);
    
    // Find or create variable in table
    int var_idx = -1;
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i].used && strcmp(variables[i].name, var_name) == 0) {
            var_idx = i;
            break;
        }
    }
    
    // If variable doesn't exist, create it
    if (var_idx == -1) {
        for (int i = 0; i < MAX_VARIABLES; i++) {
            if (!variables[i].used) {
                strcpy(variables[i].name, var_name);
                variables[i].reg_num = next_reg++;
                variables[i].used = 1;
                var_idx = i;
                break;
            }
        }
        
        if (var_idx == -1) {
            error("Too many variables");
        }
    }
    
    next_token(); // consume identifier
    
    // Expect assignment operator
    if (current_token.type != TOKEN_ASSIGN) {
        error("Expected assignment operator '='");
    }
    next_token(); // consume '='
    
    // Parse the expression on the right side
    int result_reg = parse_expression();
    
    // Generate code to store result in the variable's register
    printf("mv x%d, x%d\n", variables[var_idx].reg_num, result_reg);
    
    // Expect semicolon
    if (current_token.type != TOKEN_SEMICOLON) {
        error("Expected semicolon");
    }
    next_token(); // consume ';'
}

// Parse an expression (handles addition and subtraction)
int parse_expression() {
    int left = parse_term();
    
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        TokenType op = current_token.type;
        next_token(); // consume operator
        
        int right = parse_term();
        
        // Allocate a temporary register for the result
        int result_reg = next_reg++;
        
        if (op == TOKEN_PLUS) {
            printf("add x%d, x%d, x%d\n", result_reg, left, right);
        } else { // TOKEN_MINUS
            printf("sub x%d, x%d, x%d\n", result_reg, left, right);
        }
        
        left = result_reg;
    }
    
    return left;
}

// Parse a term (handles multiplication and division)
int parse_term() {
    int left = parse_factor();
    
    while (current_token.type == TOKEN_ASTERISK || current_token.type == TOKEN_SLASH) {
        TokenType op = current_token.type;
        next_token(); // consume operator
        
        int right = parse_factor();
        
        // Allocate a temporary register for the result
        int result_reg = next_reg++;
        
        if (op == TOKEN_ASTERISK) {
            printf("mul x%d, x%d, x%d\n", result_reg, left, right);  // Note: mul might not exist in our simple processor
        } else { // TOKEN_SLASH
            printf("div x%d, x%d, x%d\n", result_reg, left, right);  // Note: div might not exist in our simple processor
        }
        
        left = result_reg;
    }
    
    return left;
}

// Parse a factor (numbers, variables, or parenthesized expressions)
int parse_factor() {
    int result_reg;
    
    if (current_token.type == TOKEN_NUMBER) {
        // Load immediate value into a register
        result_reg = next_reg++;
        printf("li x%d, %d\n", result_reg, current_token.int_value);
        next_token(); // consume number
    } 
    else if (current_token.type == TOKEN_IDENTIFIER) {
        // Find the register for this variable
        char var_name[MAX_TOKEN_LENGTH];
        strcpy(var_name, current_token.value);
        
        int var_idx = -1;
        for (int i = 0; i < MAX_VARIABLES; i++) {
            if (variables[i].used && strcmp(variables[i].name, var_name) == 0) {
                var_idx = i;
                break;
            }
        }
        
        if (var_idx == -1) {
            error("Unknown variable");
        }
        
        result_reg = variables[var_idx].reg_num;
        next_token(); // consume identifier
    }
    else if (current_token.type == TOKEN_LPAREN) {
        next_token(); // consume '('
        result_reg = parse_expression();
        
        if (current_token.type != TOKEN_RPAREN) {
            error("Expected closing parenthesis");
        }
        next_token(); // consume ')'
    }
    else {
        error("Unexpected token in factor");
    }
    
    return result_reg;
}

// Print error message and exit
void error(const char *msg) {
    printf("Error: %s\n", msg);
    exit(1);
}

// Main compilation function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <source_file.c>\n", argv[0]);
        return 1;
    }
    
    // Read the source file
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate memory for source code
    char *source_code = malloc(file_size + 1);
    if (!source_code) {
        printf("Error: Could not allocate memory for source code\n");
        fclose(file);
        return 1;
    }
    
    // Read the file
    fread(source_code, 1, file_size, file);
    source_code[file_size] = '\0';
    fclose(file);
    
    // Initialize and run the compiler
    init_compiler(source_code);
    
    // Print the header for the assembly output
    printf("# Generated assembly code for 16-bit RISC-V processor\n");
    printf(".text\n");
    printf(".global _start\n");
    printf("_start:\n");
    
    // Parse the program
    parse_program();
    
    // Print exit code
    printf("li x17, 0\n");  // Load exit syscall number
    printf("ecall\n");       // Make system call to exit
    
    // Free allocated memory
    free(source_code);
    
    return 0;
}