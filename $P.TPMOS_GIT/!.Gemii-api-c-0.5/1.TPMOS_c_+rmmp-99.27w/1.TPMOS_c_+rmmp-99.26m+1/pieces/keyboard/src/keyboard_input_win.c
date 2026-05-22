#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#define HISTORY_PATH "pieces\\keyboard\\history.txt"
#define LEDGER_PATH "pieces\\keyboard\\ledger.txt"
#define MASTER_LEDGER_PATH "pieces\\master_ledger\\master_ledger.txt"

enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT = 1001, ARROW_UP = 1002, ARROW_DOWN = 1003, ESC_KEY = 27
};

volatile sig_atomic_t g_interrupted = 0;

void ctrlc_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
}

void writeCommand(int key) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *kb_ledger = fopen(LEDGER_PATH, "a");
    if (kb_ledger) {
        char key_char = (key >= 32 && key <= 126) ? key : '?';
        fprintf(kb_ledger, "[%s] KeyCaptured: %d ('%c') | Source: keyboard_win_getch\n", timestamp, key, key_char);
        fclose(kb_ledger);
    }

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(fp);
    }

    FILE *master = fopen(MASTER_LEDGER_PATH, "a");
    if (master) {
        fprintf(master, "[%s] InputReceived: key_code=%d | Source: keyboard_win_getch\n", timestamp, key);
        fclose(master);
    }
}

int main(void) {
    int ch, ch2;
    
    // Set up Ctrl+C handler
    signal(SIGINT, ctrlc_handler);

    printf("Windows Keyboard Muscle Active (_getch implementation)\n");
    printf("Press Ctrl+C or 'q' to exit.\n");

    while (1) {
        if (g_interrupted) break;

        // _getch() is blocking
        ch = _getch();

        // Special key prefix (0xE0 = 224 or 0 for some keys)
        if (ch == 0xE0 || ch == 0) {
            ch2 = _getch();  // Read actual key code
            int key = 0;
            if (ch2 == 0x48)      key = ARROW_UP;
            else if (ch2 == 0x50) key = ARROW_DOWN;
            else if (ch2 == 0x4B) key = ARROW_LEFT;
            else if (ch2 == 0x4D) key = ARROW_RIGHT;
            
            if (key != 0) {
                writeCommand(key);
            }
        } else if (ch == 'q') {
            break;
        } else if (ch == 3) {
            // Ctrl+C detected directly
            break;
        } else {
            // Standard ASCII
            writeCommand(ch);
        }
    }

    printf("Keyboard muscle shutting down.\n");
    return 0;
}
