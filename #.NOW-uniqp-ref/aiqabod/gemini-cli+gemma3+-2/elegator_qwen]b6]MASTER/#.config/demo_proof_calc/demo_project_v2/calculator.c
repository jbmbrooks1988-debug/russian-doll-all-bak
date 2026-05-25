#include <stdio.h>
#include <stdlib.h>

// Simple calculator program - Version 2
// Enhanced functionality: addition, subtraction, multiplication, division

int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

double divide(int a, int b) {
    if (b == 0) {
        printf("Error: Division by zero!\n");
        return 0.0;
    }
    return (double)a / b;
}

int main() {
    printf("Simple Calculator v2 - Enhanced Operations\n");
    printf("2 + 3 = %d\n", add(2, 3));
    printf("5 - 2 = %d\n", subtract(5, 2));
    printf("4 * 3 = %d\n", multiply(4, 3));
    printf("6 / 2 = %.2f\n", divide(6, 2));
    
    return 0;
}