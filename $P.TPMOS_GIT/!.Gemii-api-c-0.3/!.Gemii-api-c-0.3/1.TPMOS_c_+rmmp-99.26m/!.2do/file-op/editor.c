/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3
#define MAX_ROWS 1000
#define DISPLAY_FILE "display_data.txt"
#define COMMAND_FILE "commands.txt"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

int cx = 0, cy = 0;
int rx = 0;
int rowoff = 0;
int coloff = 0;
int screenrows = 0;
int screencols = 0;
int numrows = 0;
int dirty = 0;
char *filename = NULL;
char statusmsg[80] = {0};
time_t statusmsg_time = 0;
long last_command_offset = 0; // Tracks the file offset of the last processed command

int row_idxs[MAX_ROWS];
int row_sizes[MAX_ROWS];
int row_rsizes[MAX_ROWS];
char *row_chars[MAX_ROWS];

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorUpdateDisplayFile();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorProcessCommand(const char *command);

/*** terminal ***/

void die(const char *s) {
  perror(s);
  exit(1);
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** row operations ***/

int editorRowCxToRx(int row_idx, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row_chars[row_idx][j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(int row_idx) {
  // Simplified for file output, just store raw chars
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > numrows) return;

  memmove(&row_idxs[at + 1], &row_idxs[at], sizeof(int) * (numrows - at));
  memmove(&row_sizes[at + 1], &row_sizes[at], sizeof(int) * (numrows - at));
  memmove(&row_chars[at + 1], &row_chars[at], sizeof(char *) * (numrows - at));

  for (int j = at + 1; j <= numrows; j++) row_idxs[j]++;

  row_idxs[at] = at;
  row_sizes[at] = len;
  row_chars[at] = malloc(len + 1);
  memcpy(row_chars[at], s, len);
  row_chars[at][len] = '\0';
  numrows++;
  dirty++;
}

void editorFreeRow(int row_idx) {
  free(row_chars[row_idx]);
}

void editorDelRow(int at) {
  if (at < 0 || at >= numrows) return;
  editorFreeRow(at);
  memmove(&row_idxs[at], &row_idxs[at + 1], sizeof(int) * (numrows - at - 1));
  memmove(&row_sizes[at], &row_sizes[at + 1], sizeof(int) * (numrows - at - 1));
  memmove(&row_chars[at], &row_chars[at + 1], sizeof(char *) * (numrows - at - 1));
  for (int j = at; j < numrows - 1; j++) row_idxs[j]--;
  numrows--;
  dirty++;
}

void editorRowInsertChar(int row_idx, int at, int c) {
  if (at < 0 || at > row_sizes[row_idx]) at = row_sizes[row_idx];
  row_chars[row_idx] = realloc(row_chars[row_idx], row_sizes[row_idx] + 2);
  memmove(&row_chars[row_idx][at + 1], &row_chars[row_idx][at], row_sizes[row_idx] - at + 1);
  row_sizes[row_idx]++;
  row_chars[row_idx][at] = c;
  dirty++;
}

void editorRowAppendString(int row_idx, char *s, size_t len) {
  row_chars[row_idx] = realloc(row_chars[row_idx], row_sizes[row_idx] + len + 1);
  memcpy(&row_chars[row_idx][row_sizes[row_idx]], s, len);
  row_sizes[row_idx] += len;
  row_chars[row_idx][row_sizes[row_idx]] = '\0';
  dirty++;
}

void editorRowDelChar(int row_idx, int at) {
  if (at < 0 || at >= row_sizes[row_idx]) return;
  memmove(&row_chars[row_idx][at], &row_chars[row_idx][at + 1], row_sizes[row_idx] - at);
  row_sizes[row_idx]--;
  dirty++;
}

/*** editor operations ***/

void editorInsertChar(int c) {
  if (cy == numrows) {
    editorInsertRow(numrows, "", 0);
  }
  editorRowInsertChar(cy, cx, c);
  cx++;
}

void editorInsertNewline() {
  if (cx == 0) {
    editorInsertRow(cy, "", 0);
  } else {
    editorInsertRow(cy + 1, &row_chars[cy][cx], row_sizes[cy] - cx);
    row_sizes[cy] = cx;
    row_chars[cy][row_sizes[cy]] = '\0';
  }
  cy++;
  cx = 0;
}

void editorDelChar() {
  if (cy == numrows) return;
  if (cx == 0 && cy == 0) return;

  if (cx > 0) {
    editorRowDelChar(cy, cx - 1);
    cx--;
  } else {
    cx = row_sizes[cy - 1];
    editorRowAppendString(cy - 1, row_chars[cy], row_sizes[cy]);
    editorDelRow(cy);
    cy--;
  }
}

/*** file i/o ***/

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < numrows; j++)
    totlen += row_sizes[j] + 1;
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < numrows; j++) {
    memcpy(p, row_chars[j], row_sizes[j]);
    p += row_sizes[j];
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *new_filename) {
  free(filename);
  filename = strdup(new_filename);

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  dirty = 0;
}

void editorSave() {
  if (filename == NULL) {
    filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }

  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** output ***/

void editorScroll() {
  rx = 0;
  if (cy < numrows) {
    rx = editorRowCxToRx(cy, cx);
  }

  if (cy < rowoff) {
    rowoff = cy;
  }
  if (cy >= rowoff + screenrows) {
    rowoff = cy - screenrows + 1;
  }
  if (rx < coloff) {
    coloff = rx;
  }
  if (rx >= coloff + screencols) {
    coloff = rx - screencols + 1;
  }
}

void editorUpdateDisplayFile() {
  FILE *fp = fopen(DISPLAY_FILE, "w");
  if (!fp) die("fopen display file");

  fprintf(fp, "ROWS:%d\n", screenrows);
  fprintf(fp, "COLS:%d\n", screencols);
  fprintf(fp, "CURSOR:%d,%d\n", cy - rowoff + 1, rx - coloff + 1);
  fprintf(fp, "STATUS:%s\n", statusmsg);

  int y;
  for (y = 0; y < screenrows; y++) {
    int filerow = y + rowoff;
    if (filerow >= numrows) {
      if (numrows == 0 && y == screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
        fprintf(fp, "ROW:%s\n", welcome);
      } else {
        fprintf(fp, "ROW:~\n");
      }
    } else {
      int len = row_sizes[filerow] - coloff;
      if (len < 0) len = 0;
      if (len > screencols) len = screencols;
      char *row = malloc(len + 1);
      memcpy(row, &row_chars[filerow][coloff], len);
      row[len] = '\0';
      fprintf(fp, "ROW:%s\n", row);
      free(row);
    }
  }

  char status[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    filename ? filename : "[No Name]", numrows, dirty ? "(modified)" : "");
  fprintf(fp, "STATUSBAR:%s\n", status);

  fclose(fp);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(statusmsg, sizeof(statusmsg), fmt, ap);
  va_end(ap);
  statusmsg_time = time(NULL);
}

/*** input ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';
  long prompt_offset = last_command_offset;

  FILE *fp;
  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorUpdateDisplayFile();

    fp = fopen(COMMAND_FILE, "r");
    if (fp) {
      fseek(fp, prompt_offset, SEEK_SET);
      char command[32];
      if (fgets(command, sizeof(command), fp)) {
        command[strcspn(command, "\n")] = 0;
        int c = atoi(command);
        prompt_offset = ftell(fp);

        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
          if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
          editorSetStatusMessage("");
          free(buf);
          fclose(fp);
          last_command_offset = prompt_offset;
          return NULL;
        } else if (c == '\r') {
          if (buflen != 0) {
            editorSetStatusMessage("");
            fclose(fp);
            last_command_offset = prompt_offset;
            return buf;
          }
        } else if (!iscntrl(c) && c < 128) {
          if (buflen == bufsize - 1) {
            bufsize *= 2;
            buf = realloc(buf, bufsize);
          }
          buf[buflen++] = c;
          buf[buflen] = '\0';
        }
      }
      fclose(fp);
    }
    usleep(10000); // 100ms delay to avoid busy-waiting//100000
  }
}

void editorMoveCursor(int key) {
  int rowlen = (cy >= numrows) ? 0 : row_sizes[cy];

  switch (key) {
    case ARROW_LEFT:
      if (cx != 0) cx--;
      else if (cy > 0) { cy--; cx = row_sizes[cy]; }
      break;
    case ARROW_RIGHT:
      if (rowlen && cx < rowlen) cx++;
      else if (rowlen && cx == rowlen) { cy++; cx = 0; }
      break;
    case ARROW_UP:
      if (cy != 0) cy--;
      break;
    case ARROW_DOWN:
      if (cy < numrows) cy++;
      break;
  }

  rowlen = (cy >= numrows) ? 0 : row_sizes[cy];
  if (cx > rowlen) cx = rowlen;
}

void editorProcessCommand(const char *command) {
  static int quit_times = KILO_QUIT_TIMES;
  int c = atoi(command);

  switch (c) {
    case '\r':
      editorInsertNewline();
      break;
    case CTRL_KEY('q'):
      if (dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. "
          "Press Ctrl-Q %d more times to quit.", quit_times);
        quit_times--;
        return;
      }
      exit(0);
      break;
    case CTRL_KEY('s'):
      editorSave();
      break;
    case HOME_KEY:
      cx = 0;
      break;
    case END_KEY:
      if (cy < numrows) cx = row_sizes[cy];
      break;
    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) cy = rowoff;
        else if (c == PAGE_DOWN) { cy = rowoff + screenrows - 1; if (cy > numrows) cy = numrows; }
        int times = screenrows;
        while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
    default:
      if (c >= 0 && c < 128) editorInsertChar(c);
      break;
  }
  quit_times = KILO_QUIT_TIMES;
}

/*** init ***/

void initEditor() {
  cx = 0;
  cy = 0;
  rx = 0;
  rowoff = 0;
  coloff = 0;
  numrows = 0;
  dirty = 0;
  filename = NULL;
  statusmsg[0] = '\0';
  statusmsg_time = 0;
  last_command_offset = 0;

  if (getWindowSize(&screenrows, &screencols) == -1) die("getWindowSize");
  screenrows -= 2;
}

int main(int argc, char *argv[]) {
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  while (1) {
    editorScroll();
    editorUpdateDisplayFile();

    FILE *fp = fopen(COMMAND_FILE, "r");
    if (fp) {
      fseek(fp, last_command_offset, SEEK_SET);
      char command[32];
      while (fgets(command, sizeof(command), fp)) {
        command[strcspn(command, "\n")] = 0;
        editorProcessCommand(command);
        last_command_offset = ftell(fp);
      }
      fclose(fp);
    }
    usleep(16667); // 100ms delay to avoid busy-waiting
  }

  return 0;
}
