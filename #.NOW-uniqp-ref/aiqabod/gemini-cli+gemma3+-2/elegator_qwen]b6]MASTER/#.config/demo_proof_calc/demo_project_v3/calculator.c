#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Simple calculator program - Version 3
// Full functionality: all basic operations plus advanced functions

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

double power(double base, double exponent) {
    return pow(base, exponent);
}

double square_root(double number) {
    if (number < 0) {
        printf("Error: Cannot calculate square root of negative number!\n");
        return 0.0;
    }
    return sqrt(number);
}

int main() {
    printf("Advanced Calculator v3 - Complete Operations\n");
    printf("2 + 3 = %d\n", add(2, 3));
    printf("5 - 2 = %d\n", subtract(5, 2));
    printf("4 * 3 = %d\n", multiply(4, 3));
    printf("6 / 2 = %.2f\n", divide(6, 2));
    printf("2 ^ 3 = %.2f\n", power(2.0, 3.0));
    printf("sqrt(16) = %.2f\n", square_root(16));
    
    return 0;
}