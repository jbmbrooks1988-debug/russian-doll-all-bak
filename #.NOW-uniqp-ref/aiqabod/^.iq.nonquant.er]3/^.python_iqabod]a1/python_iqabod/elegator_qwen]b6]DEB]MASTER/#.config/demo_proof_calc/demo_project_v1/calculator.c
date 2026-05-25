#include <stdio.h>
#include <stdlib.h>

// Simple calculator program - Version 1
// Basic functionality: addition and subtraction

int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int main() {
    printf("Simple Calculator v1 - Basic Operations\n");
    printf("2 + 3 = %d\n", add(2, 3));
    printf("5 - 2 = %d\n", subtract(5, 2));
    
    return 0;
}