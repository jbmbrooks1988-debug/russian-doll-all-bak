/* khtpm_taskbar_settings_render.c — "Settings" window for the taskbar's
 * HQ menu (cell 1). Lets the player pick a PRIMARY (theme_bg) and
 * SECONDARY (theme_fg) color from a fixed palette, writes the choice to
 * #.desktop/livedesk_theme.pdl (the same file khtpm_taskbar_manager.c's
 * load_theme() already reads at startup - COLOR | bg | #rrggbb / COLOR |
 * fg | #rrggbb, this file's own real, already-wired persistence point,
 * not a new one), then triggers the existing scoped restart
 * (run_khtpm_strip.sh new - same script the HQ menu's own "$.restart"
 * row already runs) so the new theme takes effect immediately.
 *
 * Reuses every proven house pattern rather than reinventing: managed
 * window + _MOTIF_WM_HINTS (not override_redirect), RGB compose->present
 * (XGetImage->XPutImage), wraith-alpha nav (bracket badges, digit-jump
 * with the real multi-digit accumulator ported into ai-cell earlier
 * 2026-08-13 - see that file's own g_digit_accum for the reference this
 * one copies), Xft text, PNG+receipt dump for relay-testable
 * verification, pidfile+SIGTERM for clean shutdown, single-instance
 * guard in button.sh (not in this binary itself).
 *
 * Usage: khtpm_taskbar_settings_render.+x <house_root>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#define WIN_W 420
#define WIN_H 280
#define CHROME_H 28
#define SWATCH 34
#define SWATCH_GAP 8
#define SWATCH_COLS 6

static char g_house_root[PATH_BUF];
static char g_audit_dir[PATH_BUF];
static char g_pid_path[PATH_BUF];

static Display *dpy;
static Window win;
static GC gc;
static Pixmap buf;
static Visual *vis;
static Colormap cmap;
static XftDraw *xftdraw_buf;
static XftFont *font_small, *font_med;
static XftColor col_text, col_muted, col_accent, col_danger;
static Atom wm_delete;
static int g_win_w = WIN_W, g_win_h = WIN_H;
static int g_running = 0;

/* Fixed palette - hand-picked, readable-as-taskbar-bg/fg spread, not
 * exhaustive. Index 0 doubles as "back to default". */
typedef struct { const char *hex; const char *name; } Swatch;
static const Swatch g_palette[] = {
    { "#000000", "black" },  { "#ffffff", "white" },  { "#1a1a1a", "charcoal" },
    { "#e5e5e5", "silver" }, { "#ef4444", "red" },    { "#f97316", "orange" },
    { "#eab308", "yellow" }, { "#22c55e", "green" },  { "#06b6d4", "cyan" },
    { "#3b82f6", "blue" },   { "#8b5cf6", "purple" }, { "#ec4899", "pink" },
};
static const int g_n_swatches = sizeof(g_palette) / sizeof(g_palette[0]);

/* nav: swatch grid (0..n_swatches-1) + close button (last) */
typedef struct { int x, y, w, h; } NavRect;
#define MAX_NAV 32
static NavRect g_nav[MAX_NAV];
static int g_n_nav = 0;
static int g_focus_nav = 1;
static int g_digit_accum = 0; /* house standard, ported from chtpm_parser.c via ai-cell's own port earlier today */

/* phase 0 = picking primary(bg), phase 1 = picking secondary(fg), phase 2 = done/closing */
static int g_phase = 0;
static int g_chosen_bg_idx = -1;
static int g_chosen_fg_idx = -1;

static volatile sig_atomic_t g_want_exit = 0;
static void handle_sigterm(int sig) { (void)sig; g_want_exit = 1; }

static void write_pidfile(void) {
    FILE *f = fopen(g_pid_path, "w");
    if (f) { fprintf(f, "%d\n", (int)getpid()); fclose(f); }
}
static void unlink_pidfile(void) { remove(g_pid_path); }

static int nonfatal_x_error(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

static void xft_color(const char *hexname, XftColor *out) {
    XftColorAllocName(dpy, vis, cmap, hexname, out);
}

/* Real house font convention (ai-cell's own load_fonts(), matched
 * exactly - "sans-11"/"sans-13" lowercase family names don't resolve
 * via Xft and silently fall back to a huge default bitmap font, which
 * is what made this window's text look wrong-sized live). */
static void load_fonts(void) {
    font_small = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans-8");
    font_med = XftFontOpenName(dpy, DefaultScreen(dpy), "Sans-9");
}

static void draw_text(XftFont *f, XftColor *c, int x, int y, const char *s) {
    XftDrawStringUtf8(xftdraw_buf, c, f, x, y, (const FcChar8 *)s, (int)strlen(s));
}

/* Writes ONLY the bg/fg COLOR keys into livedesk_theme.pdl, preserving
 * any other COLOR rows already there (e.g. "opacity") - read-modify-
 * write, same care as the manager's own strip_state.txt tmp+rename
 * pattern, not a blind overwrite that would drop unrelated keys. */
static void apply_theme(const char *bg_hex, const char *fg_hex) {
    char path[PATH_BUF], tmp[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", g_house_root);
    snprintf(tmp, sizeof(tmp), "%s/#.desktop/livedesk_theme.pdl.tmp", g_house_root);

    char kept[8][256];
    int n_kept = 0;
    FILE *rf = fopen(path, "r");
    if (rf) {
        char line[256];
        while (fgets(line, sizeof(line), rf) && n_kept < 8) {
            if (strncmp(line, "COLOR", 5) != 0) continue;
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = strchr(p, '|');
            if (!end) continue;
            char key[16];
            size_t klen = (size_t)(end - p);
            while (klen && p[klen - 1] == ' ') klen--;
            if (klen >= sizeof(key)) continue;
            memcpy(key, p, klen); key[klen] = 0;
            if (strcmp(key, "bg") == 0 || strcmp(key, "fg") == 0) continue; /* replaced below */
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(kept[n_kept], sizeof(kept[n_kept]), "%s", line);
            n_kept++;
        }
        fclose(rf);
    }

    FILE *wf = fopen(tmp, "w");
    if (!wf) return;
    fputs("SECTION      | KEY                | VALUE\n----------------------------------------\n", wf);
    fprintf(wf, "COLOR        | bg                   | %s\n", bg_hex);
    fprintf(wf, "COLOR        | fg                   | %s\n", fg_hex);
    for (int i = 0; i < n_kept; i++) fprintf(wf, "%s\n", kept[i]);
    fclose(wf);
    remove(path);
    rename(tmp, path);

    /* Same scoped restart the HQ menu's own "$.restart" row already
     * runs - kills/rebuilds/relaunches ONLY the strip parser/manager
     * pair, never legacy or anything else in autostart.pdl, so the new
     * theme takes effect live without a full house restart. */
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "setsid nohup sh '%s/*.monads/*.livedesk-taskbar/ops/run_khtpm_strip.sh' new >/dev/null 2>&1 &",
             g_house_root);
    int rc = system(cmd);
    (void)rc;
}

static unsigned long parse_hex_pixel(const char *hex) {
    XColor c;
    if (XParseColor(dpy, cmap, hex, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    return BlackPixel(dpy, DefaultScreen(dpy));
}

static void redraw(void) {
    g_n_nav = 0;
    XSetForeground(dpy, gc, 0x141414);
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h);

    XSetForeground(dpy, gc, 0x1a1a1a);
    XFillRectangle(dpy, buf, gc, 0, 0, (unsigned)g_win_w, CHROME_H);
    draw_text(font_small, &col_muted, 10, CHROME_H - 9, "taskbar settings");

    const char *title = g_phase == 0 ? "Pick PRIMARY, then Enter"
                       : g_phase == 1 ? "Pick SECONDARY, then Enter"
                       : "Applied - closing...";
    draw_text(font_med, &col_text, 16, CHROME_H + 26, title);

    int x0 = 16, y0 = CHROME_H + 44;
    for (int i = 0; i < g_n_swatches; i++) {
        int col = i % SWATCH_COLS, row = i / SWATCH_COLS;
        int x = x0 + col * (SWATCH + SWATCH_GAP);
        int y = y0 + row * (SWATCH + SWATCH_GAP);
        int nav_i = ++g_n_nav;
        g_nav[nav_i - 1].x = x; g_nav[nav_i - 1].y = y;
        g_nav[nav_i - 1].w = SWATCH; g_nav[nav_i - 1].h = SWATCH;

        XSetForeground(dpy, gc, parse_hex_pixel(g_palette[i].hex));
        XFillRectangle(dpy, buf, gc, x, y, SWATCH, SWATCH);

        int on = (nav_i == g_focus_nav);
        int chosen = (i == g_chosen_bg_idx) || (i == g_chosen_fg_idx);
        XSetForeground(dpy, gc, on ? 0xffffff : (chosen ? 0x22c55e : 0x444444));
        XDrawRectangle(dpy, buf, gc, x - 2, y - 2, SWATCH + 4, SWATCH + 4);
        if (on) XDrawRectangle(dpy, buf, gc, x - 3, y - 3, SWATCH + 6, SWATCH + 6);

        char badge[8];
        snprintf(badge, sizeof(badge), "%d", nav_i);
        draw_text(font_small, &col_muted, x + 2, y + SWATCH + 12, badge);
    }

    if (g_chosen_bg_idx >= 0) {
        char line[64];
        snprintf(line, sizeof(line), "primary: %s", g_palette[g_chosen_bg_idx].name);
        draw_text(font_small, &col_accent, x0, y0 + 2 * (SWATCH + SWATCH_GAP) + 20, line);
    }
    if (g_chosen_fg_idx >= 0) {
        char line[64];
        snprintf(line, sizeof(line), "secondary: %s", g_palette[g_chosen_fg_idx].name);
        draw_text(font_small, &col_accent, x0, y0 + 2 * (SWATCH + SWATCH_GAP) + 38, line);
    }

    /* close button - LAST nav index, same convention as ai-cell/db-hq/events-hq */
    int nav_close = ++g_n_nav;
    g_nav[nav_close - 1].x = g_win_w - 60; g_nav[nav_close - 1].y = 0;
    g_nav[nav_close - 1].w = 60; g_nav[nav_close - 1].h = CHROME_H;
    char badge[24];
    int on = (nav_close == g_focus_nav);
    snprintf(badge, sizeof(badge), "[%s]%d. x", on ? ">" : " ", nav_close);
    draw_text(font_small, on ? &col_danger : &col_muted, g_win_w - 56, CHROME_H - 9, badge);

    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav;
    if (g_focus_nav < 1) g_focus_nav = 1;

    XSync(dpy, False);
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (frame) { XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h); XDestroyImage(frame); }
    XFlush(dpy);
}

static void dump_frame_png(void) {
    char png[PATH_BUF], receipt[PATH_BUF];
    snprintf(png, sizeof(png), "%s/settings-frame.png", g_audit_dir);
    snprintf(receipt, sizeof(receipt), "%s/settings-frame.png.receipt.txt", g_audit_dir);
    XSync(dpy, False);
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)g_win_w, (unsigned)g_win_h, AllPlanes, ZPixmap);
    if (!img) return;
    int w = g_win_w, h = g_win_h;
    unsigned char *rgb = malloc((size_t)w * h * 3);
    if (!rgb) { XDestroyImage(img); return; }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        unsigned long px = XGetPixel(img, x, y);
        size_t o = ((size_t)y * w + x) * 3;
        rgb[o] = (unsigned char)((px >> 16) & 0xff);
        rgb[o + 1] = (unsigned char)((px >> 8) & 0xff);
        rgb[o + 2] = (unsigned char)(px & 0xff);
    }
    XDestroyImage(img);
    int ok = stbi_write_png(png, w, h, 3, rgb, w * 3);
    free(rgb);
    FILE *rf = fopen(receipt, "w");
    if (rf) {
        fprintf(rf, "ok=%d w=%d h=%d t=%ld nav=%d n_nav=%d phase=%d bg_idx=%d fg_idx=%d\n",
                ok, w, h, (long)time(NULL), g_focus_nav, g_n_nav, g_phase, g_chosen_bg_idx, g_chosen_fg_idx);
        fclose(rf);
    }
}

static void activate_focused(void) {
    if (g_focus_nav < 1 || g_focus_nav > g_n_nav) return;
    if (g_focus_nav == g_n_nav) { g_running = 0; return; } /* close button */
    int idx = g_focus_nav - 1; /* swatch index */
    if (idx < 0 || idx >= g_n_swatches) return;
    if (g_phase == 0) {
        g_chosen_bg_idx = idx;
        g_phase = 1;
    } else if (g_phase == 1) {
        g_chosen_fg_idx = idx;
        g_phase = 2;
        apply_theme(g_palette[g_chosen_bg_idx].hex, g_palette[g_chosen_fg_idx].hex);
        g_running = 0; /* real KPI: applies + closes, matches "pick then it's done" expectation */
    }
}

static void handle_key(KeySym ks, char ch) {
    if (ch == 'p') { dump_frame_png(); return; }
    if (ch >= '0' && ch <= '9') {
        /* same real digit-accumulator standard ported into ai-cell
         * earlier today (from chtpm_parser.c) - greedy multi-digit
         * jump, fallback to fresh single digit on overflow. */
        int d = ch - '0';
        int new_val = g_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) { g_digit_accum = new_val; g_focus_nav = g_digit_accum; }
        else if (d > 0 && d <= g_n_nav) { g_digit_accum = d; g_focus_nav = g_digit_accum; }
        else g_digit_accum = 0;
        return;
    }
    if (ks == XK_Up || ks == XK_Left) { if (g_focus_nav > 1) g_focus_nav--; g_digit_accum = 0; return; }
    if (ks == XK_Down || ks == XK_Right) { if (g_focus_nav < g_n_nav) g_focus_nav++; g_digit_accum = 0; return; }
    if (ks == XK_Return || ks == XK_KP_Enter) { g_digit_accum = 0; activate_focused(); return; }
    if (ks == XK_Escape) { g_running = 0; return; }
}

static long g_relay_cursor = -1;
static void relay_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/taskbar_settings_agent_relay.txt", g_house_root);
}
static void dispatch_relay_code(int code) {
    if (code == 13) handle_key(XK_Return, 0);
    else if (code == 27) handle_key(XK_Escape, 0);
    else if (code >= 32 && code <= 126) handle_key(0, (char)code);
}
static int poll_agent_relay(void) {
    char path[PATH_BUF];
    relay_path(path, sizeof(path));
    struct stat stt;
    if (stat(path, &stt) != 0) return 0;
    if (g_relay_cursor < 0) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size < g_relay_cursor) { g_relay_cursor = stt.st_size; return 0; }
    if (stt.st_size == g_relay_cursor) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, g_relay_cursor, SEEK_SET);
    char line[32];
    long consumed = g_relay_cursor;
    int n_dispatched = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (!nl) break;
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) { dispatch_relay_code(code); n_dispatched++; }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return n_dispatched;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: khtpm_taskbar_settings_render.+x <house_root>\n"); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    /* #.desktop/ always exists (every khtpm/-hq window's relay files
     * already live there) - avoids needing a multi-level mkdir -p for
     * a pieces/audit/ tree that doesn't exist yet under this app's own
     * directory. */
    snprintf(g_audit_dir, sizeof(g_audit_dir), "%s/#.desktop/taskbar-settings-audit", g_house_root);
    snprintf(g_pid_path, sizeof(g_pid_path), "%s/taskbar-settings.pid", g_audit_dir);
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);
    mkdir(g_audit_dir, 0755);
    write_pidfile();

    XSetErrorHandler(nonfatal_x_error);
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "taskbar-settings: cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    vis = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    cmap = XCreateColormap(dpy, RootWindow(dpy, screen), vis, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
    swa.background_pixel = 0x141414;
    swa.border_pixel = 0;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), 200, 200, (unsigned)g_win_w, (unsigned)g_win_h,
                         0, depth, InputOutput, vis, CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, &swa);
    XStoreName(dpy, win, "taskbar-settings");

    /* Live bug report 2026-08-13: "windows that get opened (like settings)
     * open to high in the task bar" - root cause: XCreateWindow's x/y are
     * only a REQUEST; without PPosition/USPosition size hints most window
     * managers ignore them and place undecorated windows at (0,0), which
     * lands directly under/behind the taskbar's own top strip. Setting real
     * WM_NORMAL_HINTS here makes the WM honor the requested (200,200) spot,
     * comfortably clear of strip_y_offset (40-50px, see livedesk_taskbar.pdl). */
    XSizeHints *size_hints = XAllocSizeHints();
    if (size_hints) {
        size_hints->flags = PPosition | USPosition | PSize;
        size_hints->x = 200;
        size_hints->y = 200;
        size_hints->width = g_win_w;
        size_hints->height = g_win_h;
        XSetWMNormalHints(dpy, win, size_hints);
        XFree(size_hints);
    }

    Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    struct { unsigned long flags, functions, decorations; long input_mode; unsigned long status; } mwm = {2, 0, 0, 0, 0};
    XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)&mwm, 5);

    /* Real WM here is mutter/Xwayland (confirmed 2026-08-13 while chasing
     * the "opens too high" report) - Wayland compositors treat initial
     * XCreateWindow x/y and WM_NORMAL_HINTS PPosition as advisory at best,
     * unlike a plain X11 WM, so the window kept landing near (0,0) under
     * the taskbar's own top strip regardless of the hints set above. Tag
     * it _NET_WM_WINDOW_TYPE_DIALOG (mutter honors an explicit re-move for
     * dialogs far more reliably than for a plain top-level) and re-assert
     * the position with XMoveWindow after mapping, not just at create time. */
    Atom win_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom win_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, win_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&win_type_dialog, 1);

    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XMapWindow(dpy, win);
    XMoveWindow(dpy, win, 200, 260);
    XFlush(dpy);

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)g_win_w, (unsigned)g_win_h, (unsigned)depth);
    xftdraw_buf = XftDrawCreate(dpy, buf, vis, cmap);

    load_fonts();
    xft_color("#ececec", &col_text);
    xft_color("#a0a0a0", &col_muted);
    xft_color("#22c55e", &col_accent);
    xft_color("#ef4444", &col_danger);

    redraw();

    if (argc > 2 && strcmp(argv[2], "--dump-and-exit") == 0) {
        unlink_pidfile();
        dump_frame_png();
        return 0;
    }

    g_running = 1;
    while (g_running && !g_want_exit) {
        struct timeval tv = {0, 150000};
        fd_set fds; FD_ZERO(&fds); int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        select(xfd + 1, &fds, NULL, NULL, &tv);

        int need_redraw = 0;
        if (poll_agent_relay() > 0) need_redraw = 1;
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            need_redraw = 1;
            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) g_running = 0;
            else if (ev.type == KeyPress) {
                char buf_ch[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf_ch, sizeof(buf_ch), &ks, NULL);
                handle_key(ks, n > 0 ? buf_ch[0] : 0);
            } else if (ev.type == ButtonPress) {
                for (int i = 0; i < g_n_nav; i++) {
                    if (ev.xbutton.x >= g_nav[i].x && ev.xbutton.x < g_nav[i].x + g_nav[i].w &&
                        ev.xbutton.y >= g_nav[i].y && ev.xbutton.y < g_nav[i].y + g_nav[i].h) {
                        g_focus_nav = i + 1;
                        activate_focused();
                        break;
                    }
                }
            }
        }
        if (need_redraw) redraw();
    }

    unlink_pidfile();
    XftDrawDestroy(xftdraw_buf);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, buf);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
