#include <stdio.h>

int main() {
    int turn[1] = {1}; // Array to store turn number
    char ch;

    printf("turn %d ending, turn %d beginning\n", turn[0], turn[0] + 1);
    fflush(stdout); // Ensure prompt is sent

    // Read input character by character until EOF or interrupted
    while ((ch = getchar()) != EOF) {
        printf("%d\n", (int)ch); // Print ASCII value of the character
        fflush(stdout); // Ensure output is displayed immediately
    }

    return 0;
}
