#include <stdio.h>
#include <termios.h> // Required for non-canonical input
#include <unistd.h>  // Required for read()

int main() {
    struct termios old_tio, new_tio;
    char ch;

    // Get the current terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);

    // Set new terminal settings for non-canonical input (raw mode)
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

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

    // Restore original terminal settings (important for clean exit)
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

    return 0;
}
