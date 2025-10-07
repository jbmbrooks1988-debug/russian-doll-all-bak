#include <stdio.h>
#include <termios.h> // Required for non-canonical input
#include <unistd.h>  // Required for read()

int main() {
 
    char ch;

    printf("Press Enter to print 'Hello World' (Ctrl+C to exit).\n");

    while (1) {
        // Read a single character
        read(STDIN_FILENO, &ch, 1);
        
        //////////////////
        printf("%d\n", (int)ch); // Print ASCII value of the character
        fflush(stdout); // Ensure output is displayed immediately
/////////////

        // Check if the character is the Enter key (newline character)
        if (ch == '\n') {
            printf("Hello World!\n");
        }
    }

  

    return 0;
}
