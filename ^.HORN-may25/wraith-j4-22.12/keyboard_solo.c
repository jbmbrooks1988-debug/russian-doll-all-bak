#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

/* SOLO CONFIG: Write to current directory */
#define HISTORY_PATH "history.txt"
#define LEDGER_PATH "ledger.txt"
#define MASTER_LEDGER_PATH "master_ledger.txt"

enum editorKey {
    ARROW_LEFT = 1000, ARROW_RIGHT = 1001, ARROW_UP = 1002, ARROW_DOWN = 1003, ESC_KEY = 27
};

struct termios orig_termios;
int tty_fd = -1;

void disableRawMode() {
    if (tty_fd >= 0) {
        /* Disable mouse tracking (1003l = Any event, 1006l = SGR) */
        write(tty_fd, "\x1b[?1003l\x1b[?1006l", 16);
        tcflush(tty_fd, TCIFLUSH);
        tcsetattr(tty_fd, TCSAFLUSH, &orig_termios);
        close(tty_fd);
        tty_fd = -1;
    }
}

void handle_signal(int sig) {
    (void)sig;
    disableRawMode();
    printf("\n[SOLO] Interrupted. Control safely returned.\n");
    exit(0);
}

void enableRawMode() {
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd == -1) {
        perror("open /dev/tty");
        exit(1);
    }
    tcgetattr(tty_fd, &orig_termios);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(tty_fd, TCSAFLUSH, &raw);

    /* Enable any-event and SGR mouse reporting */
    write(tty_fd, "\x1b[?1003h\x1b[?1006h", 16);
}

void writeMouseCommand(int btn, int x, int y) {
    /* VISUAL FEEDBACK (Like mouse-00.01.c) */
    fprintf(stderr, "\r\033[K[SOLO MOUSE] BTN: %d, X: %d, Y: %d", btn, x, y);
    fflush(stderr);

    time_t rawtime; struct tm *timeinfo; char timestamp[100];
    time(&rawtime); timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        fprintf(fp, "[%s] MOUSE_EVENT: %d %d %d\n", timestamp, btn, x, y);
        fclose(fp);
    }
}

int readKey() {
    int nread;
    char c;
    while ((nread = read(tty_fd, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) return -1;
        usleep(10000);
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(tty_fd, &seq[0], 1) != 1) return '\x1b';
        if (read(tty_fd, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] == '<') {
                /* SGR Mouse Mode parsing */
                int b = 0, x = 0, y = 0;
                char ch;
                while (read(tty_fd, &ch, 1) == 1 && ch != ';') {
                    if (ch >= '0' && ch <= '9') b = b * 10 + (ch - '0');
                }
                while (read(tty_fd, &ch, 1) == 1 && ch != ';') {
                    if (ch >= '0' && ch <= '9') x = x * 10 + (ch - '0');
                }
                while (read(tty_fd, &ch, 1) == 1 && ch != 'M' && ch != 'm') {
                    if (ch >= '0' && ch <= '9') y = y * 10 + (ch - '0');
                }
                writeMouseCommand(b, x, y);
                return 0; 
            }
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
        return '\x1b';
    }
    return (unsigned char)c;
}

void writeCommand(int key) {
    if (key == 0) return;
    
    /* VISUAL FEEDBACK */
    fprintf(stderr, "\r\033[K[SOLO KEY] Code: %d", key);
    fflush(stderr);

    time_t rawtime; struct tm *timeinfo; char timestamp[100];
    time(&rawtime); timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    FILE *fp = fopen(HISTORY_PATH, "a");
    if (fp) {
        fprintf(fp, "[%s] KEY_PRESSED: %d\n", timestamp, key);
        fclose(fp);
    }
}

int main() {
    enableRawMode();
    printf("SOLO Mouse/Keyboard Tester Active.\n");
    printf("Writing to ./history.txt\n");
    printf("Press 'q' or Ctrl+C to exit.\n\n");
    
    while (1) {
        int c = readKey();
        if (c == -1) continue;
        if (c == 'q') break;
        writeCommand(c);
    }
    return 0;
}
