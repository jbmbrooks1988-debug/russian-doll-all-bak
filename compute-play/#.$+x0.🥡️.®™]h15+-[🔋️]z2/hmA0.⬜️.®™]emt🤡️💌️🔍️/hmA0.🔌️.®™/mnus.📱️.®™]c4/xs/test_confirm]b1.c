#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For getpid()

int main() {
    FILE *debug_fp = fopen("test_confirm_debug.log", "a");
    if (!debug_fp) {
        fprintf(stderr, "Error: Cannot open test_confirm_debug.log\n");
        return 1;
    }
    fprintf(debug_fp, "[PID %d] Starting test_confirm\n", getpid());
    fflush(debug_fp);

    int turn[1] = {1}; // Array to store turn number
    
    printf("turn %d ending, turn %d beginning\n", turn[0], turn[0] + 1);
    fflush(stdout); // Ensure prompt is sent
    
    int c = getchar(); // Waits for Enter key
    fprintf(debug_fp, "[PID %d] getchar() received: %d (0x%x)\n", getpid(), c, c);
    fflush(debug_fp);
    if (c == '\n') {
        fprintf(debug_fp, "[PID %d] Received newline, exiting\n", getpid());
    } else if (c == EOF) {
        fprintf(debug_fp, "[PID %d] Received EOF, exiting\n", getpid());
    } else {
        fprintf(debug_fp, "[PID %d] Received unexpected character: %d (0x%x), exiting\n", getpid(), c, c);
    }
    fclose(debug_fp);
    
    return 0;
}
