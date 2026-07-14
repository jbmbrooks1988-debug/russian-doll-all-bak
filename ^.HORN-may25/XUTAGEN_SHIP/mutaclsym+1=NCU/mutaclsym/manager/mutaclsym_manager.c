/* mutaclsym manager - the ONE process allowed to touch ncurses.
 * Per !.world_architecture+1=rusindol.txt and cdda-tpm-std-fast.txt:
 * this is intentionally thin. It does not know how to move the hero,
 * resolve collisions, or advance a turn - that logic lives in
 * pal/main_loop.pal and ops/, running in a separate prisc+x process
 * this manager spawns and only ever talks to through files:
 *   - appends raw keycodes to pieces/apps/player_app/history.txt
 *   - polls pieces/display/frame_changed.txt SIZE (not mtime) for
 *     growth, and only then re-reads state + redraws
 * No project header is shared with prisc+x, move_player, or end_turn;
 * this file defines its own constants and path logic, on purpose. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <curses.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define MAP_W 40
#define MAP_H 16

static char project_root[MAX_PATH];
static pid_t prisc_pid = -1;

static void resolve_root(void) {
    if (!getcwd(project_root, sizeof(project_root))) {
        snprintf(project_root, sizeof(project_root), ".");
    }
}

static void kill_prisc(void) {
    if (prisc_pid > 0) {
        kill(prisc_pid, SIGTERM);
        waitpid(prisc_pid, NULL, 0);
        prisc_pid = -1;
    }
}

static void spawn_prisc(void) {
    setenv("PRISC_PROJECT_ROOT", project_root, 1);
    setenv("PRISC_PROJECT_ID", "mutaclsym", 1);

    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        execl("system/prisc+x", "system/prisc+x", "pal/main_loop.pal", (char *)NULL);
        _exit(127); /* only reached if execl fails */
    }
    prisc_pid = pid;
}

static void append_key(int key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/apps/player_app/history.txt", project_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%d\n", key);
    fclose(f);
}

static long frame_marker_size(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/pieces/display/frame_changed.txt", project_root);
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static int read_kv_int(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[MAX_LINE];
    int val = def;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) { val = atoi(eq + 1); break; }
    }
    fclose(f);
    return val;
}

static void redraw(void) {
    char hero_path[MAX_PATH], map_path[MAX_PATH], turn_path[MAX_PATH];
    snprintf(hero_path, sizeof(hero_path), "%s/pieces/world_01/map_start/hero/state.txt", project_root);
    snprintf(map_path, sizeof(map_path), "%s/pieces/world_01/map_start/map.txt", project_root);
    snprintf(turn_path, sizeof(turn_path), "%s/pieces/world_01/map_start/state.txt", project_root);

    int px = read_kv_int(hero_path, "pos_x", 0);
    int py = read_kv_int(hero_path, "pos_y", 0);
    int turn = read_kv_int(turn_path, "turn", 0);

    erase();
    mvprintw(0, 0, "MUTACLSYM   turn: %d", turn);

    FILE *f = fopen(map_path, "r");
    if (f) {
        char line[MAP_W + 4];
        int row = 1;
        while (fgets(line, sizeof(line), f) && row <= MAP_H) {
            line[strcspn(line, "\n")] = '\0';
            mvprintw(row, 0, "%s", line);
            row++;
        }
        fclose(f);
    }
    mvaddch(1 + py, px, '@');
    mvprintw(MAP_H + 2, 0, "[wasd/arrows] move  [q] quit");
    refresh();
}

int main(void) {
    resolve_root();
    spawn_prisc();

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE); /* decode arrow-key escape sequences into single KEY_* codes */
    timeout(50); /* non-blocking-ish getch: unblocks every 50ms to re-check the marker */

    long last_marker = -2; /* force first redraw */
    redraw();
    last_marker = frame_marker_size();

    int running = 1;
    while (running) {
        int ch = getch();
        if (ch == 27) {
            /* Fallback for terminals/ptys that don't complete the
             * keypad()/terminfo application-cursor-key handshake: decode
             * a raw "ESC [ A/B/C/D" sequence ourselves instead of trusting
             * ncurses to have already collapsed it into a single KEY_*. */
            int c2 = getch();
            if (c2 == '[') {
                int c3 = getch();
                switch (c3) {
                    case 'A': ch = 'w'; break; /* up */
                    case 'B': ch = 's'; break; /* down */
                    case 'C': ch = 'd'; break; /* right */
                    case 'D': ch = 'a'; break; /* left */
                    default:  ch = ERR;  break; /* unrecognized escape: drop it */
                }
            } else {
                ch = ERR; /* lone ESC or unrecognized lead byte: drop it */
            }
        }
        if (ch != ERR) {
            if (ch == 'q') { running = 0; break; }
            append_key(ch);
        }
        long m = frame_marker_size();
        if (m != last_marker) {
            last_marker = m;
            redraw();
        }
    }

    endwin();
    kill_prisc();
    return 0;
}
