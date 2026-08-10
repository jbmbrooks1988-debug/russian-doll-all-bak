/* key_injector - tails an injection file and translates each new line into
 * a REAL X11 event via XTest (XTestFakeKeyEvent/XTestFakeButtonEvent) -
 * indistinguishable from physical input to any receiving client, so a
 * raw-Xlib program like &.widgits/livedesk-taskbar/ops/tp_taskbar.c needs
 * zero changes to be drivable this way. Built for the popup-focus bug
 * investigation, 2026-08-09 (see !.HOUSE_STDS.md §F-19) - vendored here so
 * the harness owns a persistent copy instead of depending on a session's
 * ephemeral job tmp storage.
 *
 * File format, one directive per line, appended to the path given on the
 * command line:
 *   KEY_PRESSED: <name>   - name is an X keysym name (Left/Right/Up/Down/
 *                           Return/Escape/BackSpace) or a single ASCII char
 *                           (digit/letter), e.g. "KEY_PRESSED: 1"
 *   CLICK: <x> <y> <button>  - moves pointer to x,y and fakes a button
 *                              press+release (button 1=left, 3=right)
 *
 * Usage: key_injector <inject_file_path> [--once]
 *   --once: drain whatever's already in the file, then exit (for
 *           scripted harness scenarios - default is to run forever,
 *           tailing for new lines, Ctrl+C to stop).
 */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINE_BUF 512

static void do_key(Display *dpy, const char *name) {
    KeySym ks = 0;
    if (strlen(name) == 1) {
        char buf[2] = { name[0], 0 };
        ks = XStringToKeysym(buf);
    } else {
        ks = XStringToKeysym(name);
    }
    if (ks == NoSymbol) {
        fprintf(stderr, "key_injector: unknown key name '%s'\n", name);
        return;
    }
    KeyCode kc = XKeysymToKeycode(dpy, ks);
    if (!kc) {
        fprintf(stderr, "key_injector: no keycode for '%s'\n", name);
        return;
    }
    XTestFakeKeyEvent(dpy, kc, True, 0);
    usleep(30000);
    XTestFakeKeyEvent(dpy, kc, False, 0);
    XSync(dpy, False);
    usleep(400000); /* generous margin past the app's own poll interval (select() timeout up to 300ms) so a focus-transfer/redraw race under system load can't silently drop a key - widened 2026-08-09 after a harness run showed two Right presses in a row occasionally not both registering at 150ms */
}

static void do_click(Display *dpy, int x, int y, int button) {
    XTestFakeMotionEvent(dpy, -1, x, y, 0);
    XSync(dpy, False);
    usleep(100000);
    XTestFakeButtonEvent(dpy, button, True, 0);
    usleep(50000);
    XTestFakeButtonEvent(dpy, button, False, 0);
    XSync(dpy, False);
    usleep(200000);
}

static void process_new_lines(FILE *f, Display *dpy) {
    char line[LINE_BUF];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "KEY_PRESSED:", 12) == 0) {
            char *p = line + 12;
            while (*p == ' ') p++;
            fprintf(stderr, "key_injector: KEY_PRESSED '%s'\n", p);
            do_key(dpy, p);
        } else if (strncmp(line, "CLICK:", 6) == 0) {
            int x, y, btn;
            if (sscanf(line + 6, "%d %d %d", &x, &y, &btn) == 3) {
                fprintf(stderr, "key_injector: CLICK %d,%d btn=%d\n", x, y, btn);
                do_click(dpy, x, y, btn);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <inject_file_path> [--once]\n", argv[0]); return 1; }
    const char *inject_path = argv[1];
    int once = (argc > 2 && strcmp(argv[2], "--once") == 0);

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "key_injector: cannot open display\n"); return 1; }

    long last_size = 0;
    if (once) {
        /* Drain from the START of the file - the harness writes its whole
         * scripted sequence first, then runs us once to play it back. */
        FILE *f = fopen(inject_path, "r");
        if (f) {
            process_new_lines(f, dpy);
            fclose(f);
        }
        return 0;
    }

    FILE *f0 = fopen(inject_path, "r");
    if (f0) { fseek(f0, 0, SEEK_END); last_size = ftell(f0); fclose(f0); }
    fprintf(stderr, "key_injector: watching %s (starting at offset %ld)\n", inject_path, last_size);

    for (;;) {
        FILE *f = fopen(inject_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long cur_size = ftell(f);
            if (cur_size > last_size) {
                fseek(f, last_size, SEEK_SET);
                process_new_lines(f, dpy);
                last_size = cur_size;
            }
            fclose(f);
        }
        usleep(100000);
    }
    return 0;
}
