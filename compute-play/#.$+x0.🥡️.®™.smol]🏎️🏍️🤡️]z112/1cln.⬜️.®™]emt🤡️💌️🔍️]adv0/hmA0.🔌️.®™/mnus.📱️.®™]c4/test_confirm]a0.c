#include <stdio.h>

int main() {
    int turn[1] = {1}; // Array to store turn number
    
    printf("turn %d ending, turn %d beginning\n", turn[0], turn[0] + 1);
     fflush(stdout); // Ensure prompt is sent
    
    char input[1];
    getchar(); // Waits for Enter key
    
    return 0;
}
