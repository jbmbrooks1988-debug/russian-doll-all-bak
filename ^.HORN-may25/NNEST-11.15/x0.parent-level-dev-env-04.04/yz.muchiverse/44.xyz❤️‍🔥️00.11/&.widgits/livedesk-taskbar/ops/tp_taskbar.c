/* LEGACY: do not add design logic here. Shared = khtpm_taskbar_core.c (+ plat_win/x11). See KHTPM-ARCH.txt */
/* tp_taskbar - real livedesk taskbar widget, 2026-08-05.
 * Direct instruction: "a taskbar-widget at bottom of the screen, is
 * something we should start experimenting with now" + "it can open
 * when livedesk using app is open, and if one is already open just
 * add the tab of that app to the taskbar."
 *
 * Usage: tp_taskbar.+x <house_root>
 *
 * REAL CORRECTION 2026-08-05, same day (see TILE_PICKER_DESIGN.md Â§13 -
 * "brackets are ment for focuz not holding numbers and already we have
 * repeat indexes in toolbar and context menu, it should look up first
 * and increment indexes so if i click toolbar then enter 'number' it
 * will jump to any open khtpm style window... there shoul be a
 * terminal style place for input... i would put input space in middle
 * of toolbar"): tabs no longer show the PERMANENT livedesk ledger index
 * (that could collide with a context-menu's own row numbers) - they now
 * claim their own number from the exact same SHARED, LIVE nav-claim
 * pool (house_root/#.desktop/livedesk_nav_claims.txt) that
 * tp_desktop_window.c's own context-menu rows claim from
 * (nav_claim_rows() there), via sync_tab_claims() here - so a tab and a
 * menu row can never show the same live number at once. A real
 * terminal-style input box sits in the MIDDLE of the bar: type a
 * number, Enter jumps - to that tab's real window if the number
 * belongs to a tab, or remotely activates a row inside some OTHER
 * window's currently-open menu (writing a real "ACTIVATE_NAV:<N>"
 * command into that entity's own interact_relay.txt) if the number
 * belongs to a menu row instead.
 *
 * A single, persistent, override_redirect bar spanning the bottom of
 * the screen. Never spawned directly by a human - tp_desktop_window.c's
 * own ensure_taskbar_running() launches exactly one instance (real
 * PID-file liveness check, house_root/#.desktop/livedesk_taskbar.pid)
 * the first time ANY livedesk entity opens, and every entity after
 * that just becomes a new tab, not a new bar. */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* ========================================================================
 * 2026-08-06 FOCUS-RECOVERY â€” taskbar Nav (user: toolbar still broken).
 * Match entity popup option C: WM_CLASS + soft focus + keyboard grab
 * while Nav is active. Locks stay off. Entity raise-steal stays off.
 * ======================================================================== */
#ifndef LIVEDESK_USE_REGISTRY_LOCK
#define LIVEDESK_USE_REGISTRY_LOCK 0
#endif
#ifndef LIVEDESK_TASKBAR_RAISE_FOCUS_ENTITIES
#define LIVEDESK_TASKBAR_RAISE_FOCUS_ENTITIES 0
#endif

#define PATH_BUF 4352
#define POLL_INTERVAL_USEC 300000
#define BAR_H 32
#define TAB_W 160
#define MAX_TABS 64

typedef struct {
    int pid;
    int nav;
    char entity[128];
    char path[PATH_BUF];
} Tab;

/* REAL, 2026-08-05, direct instruction ("i want to add a '$' icon next
 * to 'x' that will auto run $.crypt. this should have a pdl (toolbar)
 * so other shortcuts like this could be made and given a 'glyph' and
 * would allow them to fire w/e opp/widget app or w/e its pathed too"):
 * a real, generic, data-driven shortcut bar - #.desktop/
 * livedesk_shortcuts.pdl, one "SHORTCUT | <glyph> | <real command>"
 * row per icon. The "$" -> $.crypts autostart is just the FIRST real
 * row, not hardcoded specially - adding more real shortcuts later is
 * purely a data change, no code change. */
#define MAX_SHORTCUTS 16
#define SHORTCUT_W 32
typedef struct {
    char glyph[16];
    char command[PATH_BUF];
} Shortcut;

static int load_shortcuts(const char *house_root, Shortcut *out, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_shortcuts.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SHORTCUT", 8) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *glyph_end = end;
        while (glyph_end > p && glyph_end[-1] == ' ') glyph_end--;
        size_t glen = (size_t)(glyph_end - p);
        if (glen == 0 || glen >= sizeof(out[0].glyph)) continue;
        memcpy(out[n].glyph, p, glen);
        out[n].glyph[glen] = '\0';

        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        char *v_end = v + strlen(v);
        while (v_end > v && v_end[-1] == ' ') v_end--;
        *v_end = '\0';
        snprintf(out[n].command, sizeof(out[0].command), "%s", v);
        n++;
    }
    fclose(f);
    return n;
}

/* REAL, 2026-08-05, direct instruction ("i want to get color of kthpm
 * toolbar and text from a .pdl or something ... dark black and bright
 * green text, like a digital pager look"): real, data-driven theme -
 * #.desktop/livedesk_theme.pdl's own "COLOR | bg | #000000" / "COLOR |
 * fg | #00ff00" rows, parsed the same "SECTION | KEY | VALUE" shape
 * every other .pdl in this house already uses. Missing file or missing
 * row = real, documented default (white bg / black fg, today's existing
 * look), not a crash. */
static int load_theme(const char *house_root, char *bg_out, size_t bg_sz, char *fg_out, size_t fg_sz) {
    snprintf(bg_out, bg_sz, "white");
    snprintf(fg_out, fg_sz, "black");
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *key_end = end;
        while (key_end > p && key_end[-1] == ' ') key_end--;
        char key[16];
        size_t klen = (size_t)(key_end - p);
        if (klen == 0 || klen >= sizeof(key)) continue;
        memcpy(key, p, klen);
        key[klen] = '\0';

        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        char *v_end = v + strlen(v);
        while (v_end > v && v_end[-1] == ' ') v_end--;
        *v_end = '\0';
        if (v[0] == '\0') continue;

        if (strcmp(key, "bg") == 0) snprintf(bg_out, bg_sz, "%s", v);
        else if (strcmp(key, "fg") == 0) snprintf(fg_out, fg_sz, "%s", v);
    }
    fclose(f);
    return 1;
}

/* Real XAllocNamedColor lookup (accepts both "#rrggbb" and X11 color
 * names) - falls back to the given default pixel if the name doesn't
 * parse/allocate, never crashes on a bad .pdl value. */
static unsigned long alloc_color_or(Display *dpy, const char *name, unsigned long fallback) {
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor color;
    if (XAllocNamedColor(dpy, cmap, name, &color, &color)) return color.pixel;
    return fallback;
}

/* REAL FIX 2026-08-06, direct report ("toolbar nav is at 7, but context
 * opened at 13") - traced live: livedesk_open.txt/livedesk_nav_claims.txt
 * were full of dead-PID entries (some from ordinary churn, one line
 * flat-out corrupted from a two-process unsynchronized write), and
 * neither side ever checked a PID was actually still alive before
 * trusting it - a stale entry just sat there forever, inflating every
 * later nav count. Same self-healing fix as tp_desktop_window.c's own
 * nav_claim_rows()/livedesk_registry_add(): verify liveness here too,
 * and rewrite the file with only live entries kept, every single load. */
static int pid_is_alive(int pid) {
    if (pid <= 0) return 0;
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
}

/* REAL FIX 2026-08-06, direct-caught regression ("book-stack missing
 * from livedesk_open.txt after a simultaneous multi-launch") - see
 * tp_desktop_window.c's own registry_lock_acquire() header comment: the
 * self-healing read-prune-write-rename cycle isn't atomic across
 * processes without a real lock. Same shared lockfile, same fix here. */
static int g_registry_lock_fd = -1;
static void registry_lock_acquire(const char *house_root) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd < 0) {
        char lock_path[PATH_BUF];
        snprintf(lock_path, sizeof(lock_path), "%s/#.desktop/livedesk_registry.lock", house_root);
        g_registry_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0666);
    }
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_EX);
}
static void registry_lock_release(void) {
    if (!LIVEDESK_USE_REGISTRY_LOCK) return;
    if (g_registry_lock_fd >= 0) flock(g_registry_lock_fd, LOCK_UN);
}

static int load_tabs(const char *house_root, Tab *tabs, int max) {
    char reg_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(reg_path, sizeof(reg_path), "%s/#.desktop/livedesk_open.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_open.txt.tmp", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(reg_path, "r");
    if (!f) { registry_lock_release(); return 0; }
    FILE *w = fopen(tmp_path, "w");
    char line[PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        Tab t;
        memset(&t, 0, sizeof(t));
        char *p;
        if ((p = strstr(line, "PID=")) != NULL) t.pid = atoi(p + 4);
        if ((p = strstr(line, "ENTITY=")) != NULL) {
            char *e = p + 7;
            char *end = strchr(e, '|');
            size_t len = end ? (size_t)(end - e) : strcspn(e, "\r\n");
            if (len >= sizeof(t.entity)) len = sizeof(t.entity) - 1;
            memcpy(t.entity, e, len);
            t.entity[len] = '\0';
        }
        if ((p = strstr(line, "PATH=")) != NULL) {
            snprintf(t.path, sizeof(t.path), "%s", p + 5);
            t.path[strcspn(t.path, "\r\n")] = '\0';
        }
        if (!t.entity[0] || !pid_is_alive(t.pid)) continue; /* drop malformed/dead - self-heal */
        if (w) fputs(line, w);
        tabs[n++] = t;
    }
    fclose(f);
    if (w) { fclose(w); rename(tmp_path, reg_path); }
    registry_lock_release();
    return n;
}

static int nav_idx_of_pid(Tab *tabs, int n, int pid) {
    for (int i = 0; i < n; i++) if (tabs[i].pid == pid) return i;
    return -1;
}

/* REAL, 2026-08-05: keeps each currently-open tab's own claim in the
 * SHARED live nav pool up to date - reuses a claim already held by that
 * PID (parses it back into tabs[i].nav), drops claims for PIDs that are
 * no longer open (entity closed), and assigns a fresh number (current
 * max seen across the WHOLE shared file, tab claims and menu-row claims
 * alike, + 1) to any tab that doesn't have one yet. Row claims
 * (KIND=row, owned by tp_desktop_window.c's own open menus) are left
 * completely untouched - this function only ever manages its own
 * KIND=tab lines. */
static void sync_tab_claims(const char *house_root, Tab *tabs, int n_tabs) {
    char claims_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_nav_claims.txt.tmp", house_root);
    for (int i = 0; i < n_tabs; i++) tabs[i].nav = 0;
    int max_nav = 0;
    FILE *w = fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = fopen(claims_path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *navp = strstr(line, "NAV=");
            int nav_v = navp ? atoi(navp + 4) : 0;
            if (nav_v > max_nav) max_nav = nav_v;
            if (strncmp(line, "KIND=tab", 8) == 0) {
                char *pidp = strstr(line, "PID=");
                int pid_v = pidp ? atoi(pidp + 4) : -1;
                int idx = nav_idx_of_pid(tabs, n_tabs, pid_v);
                if (idx >= 0) {
                    tabs[idx].nav = nav_v;
                    fputs(line, w);
                }
                /* else: entity closed since this claim was made - drop it. */
            } else {
                fputs(line, w);
            }
        }
        fclose(f);
    }
    for (int i = 0; i < n_tabs; i++) {
        if (tabs[i].nav == 0) {
            max_nav++;
            tabs[i].nav = max_nav;
            fprintf(w, "KIND=tab|PID=%d|NAV=%d|ENTITY=%s|PATH=%s\n",
                    tabs[i].pid, tabs[i].nav, tabs[i].entity, tabs[i].path);
        }
    }
    fclose(w);
    rename(tmp_path, claims_path);
}

/* Looks up a real, live claim by its shared number - used by the
 * terminal input's own Enter handler to decide whether the typed
 * number is a tab (jump = raise+focus) or a menu row somewhere else
 * (jump = remote ACTIVATE_NAV injection). */
static int lookup_nav(const char *house_root, int nav_n, char *kind_out, size_t kind_sz,
                       char *entity_out, size_t entity_sz, char *path_out, size_t path_sz) {
    char claims_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *navp = strstr(line, "NAV=");
        if (!navp || atoi(navp + 4) != nav_n) continue;
        snprintf(kind_out, kind_sz, "%s", strncmp(line, "KIND=tab", 8) == 0 ? "tab" : "row");
        char *ep = strstr(line, "ENTITY=");
        if (ep) {
            char *e = ep + 7;
            char *end = strchr(e, '|');
            size_t l = end ? (size_t)(end - e) : strcspn(e, "\r\n");
            if (l >= entity_sz) l = entity_sz - 1;
            memcpy(entity_out, e, l);
            entity_out[l] = '\0';
        }
        char *pp = strstr(line, "PATH=");
        if (pp) {
            snprintf(path_out, path_sz, "%s", pp + 5);
            path_out[strcspn(path_out, "\r\n")] = '\0';
        }
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

/* REAL, 2026-08-06, direct instruction ("logged keybord input when
 * toolbar is on, and it will be sent to our khtpm from master-ledger
 * for desktop to our own .txt... focus giving us our own control of
 * this"): real, pragmatic answer to this whole session's XWayland grab
 * saga - this taskbar's own input box already reliably holds real X
 * focus via plain XSetInputFocus (no grab needed, XWayland doesn't
 * restrict that), so it becomes a remote keyboard for whichever entity
 * currently has a popup open, instead of relying on that entity's OWN
 * process winning a real XGrabKeyboard fight. "Currently has a popup
 * open" is real, live, derivable state - livedesk_nav_claims.txt only
 * ever contains KIND=row entries while nav_claim_rows()/nav_release_pid()
 * says a popup is genuinely open right now (see tp_desktop_window.c) -
 * so the first KIND=row line found IS the live popup owner, no guessing. */
/* Highest live NAV= address currently claimed (tabs + open menu rows).
 * Mirrors chtpm_parser's total_nav bound for digit accumulation. */
static int max_claimed_nav(const char *house_root) {
    char claims_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int max_n = 0;
    while (fgets(line, sizeof(line), f)) {
        char *navp = strstr(line, "NAV=");
        if (!navp) continue;
        int v = atoi(navp + 4);
        if (v > max_n) max_n = v;
    }
    fclose(f);
    return max_n;
}


static int find_open_popup_path(const char *house_root, char *path_out, size_t path_sz) {
    char claims_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "KIND=row", 8) != 0) continue;
        char *pp = strstr(line, "PATH=");
        if (!pp) continue;
        snprintf(path_out, path_sz, "%s", pp + 5);
        path_out[strcspn(path_out, "\r\n")] = '\0';
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

static Window find_by_name(Display *d, Window start, const char *target) {
    Window root, parent, *children;
    unsigned int nchildren;
    Window found = 0;
    if (!XQueryTree(d, start, &root, &parent, &children, &nchildren)) return 0;
    for (unsigned int i = 0; i < nchildren && !found; i++) {
        char *name = NULL;
        if (XFetchName(d, children[i], &name) && name) {
            if (strstr(name, target)) found = children[i];
            XFree(name);
        }
        if (!found) found = find_by_name(d, children[i], target);
    }
    if (children) XFree(children);
    return found;
}

#define CLOSE_BTN_W 40

/* REAL, 2026-08-05, direct report ("i tried to open book stack from
 * toolbar, but it doesn't have '>' in bookstack which is on toolbar"):
 * tabs never actually checked real X input focus - "[ ]" was hardcoded
 * unconditionally for every tab, regardless of which window (if any)
 * genuinely has focus right now. Real fix: query the actual currently-
 * focused window (XGetInputFocus) once per redraw and match its own
 * real title against each tab's expected "tile:<entity>-" substring -
 * same real technique find_by_name() already uses to LOCATE a window,
 * just checking the CURRENT focus instead of searching the whole tree. */
static int tab_has_focus(Display *dpy, const char *entity) {
    Window focused;
    int revert;
    XGetInputFocus(dpy, &focused, &revert);
    if (focused == None || focused == PointerRoot) return 0;
    char *name = NULL;
    int has = 0;
    if (XFetchName(dpy, focused, &name) && name) {
        char target[160];
        snprintf(target, sizeof(target), "tile:%s-", entity);
        if (strstr(name, target)) has = 1;
        XFree(name);
    }
    return has;
}

/* REAL FIX 2026-08-05, direct report ("still blinking" / "wasn't doing
 * this before color change"): this bar was drawing straight to the
 * live window - XClearArea, then several separate XDrawLine/XDrawString
 * calls, each its own round trip. The gap between "cleared" and "text
 * back" was ALWAYS there, just invisible against a near-white
 * background; solid black made it a visible flash every single redraw.
 * Real fix: real double-buffering - draw everything into an off-screen
 * Pixmap first, then one atomic XCopyArea onto the window, so the
 * window itself never shows a bare-cleared frame. buf_pixmap is created
 * once (BAR_H tall, full screen_w wide - big enough for every real
 * caller: the bottom bar itself). */
static Pixmap g_bar_buf = 0;
static GC g_bar_buf_gc = 0;

static void draw_bar(Display *dpy, Window win, GC gc, int screen_w,
                     Tab *tabs, int n_tabs, int nav_armed, int digit_buf_len,
                     Shortcut *shortcuts, int n_shortcuts, unsigned long bg_pixel, int tab_focus_idx,
                     const char *digit_buf) {
    Window win_real = win;
    if (!g_bar_buf) {
        g_bar_buf = XCreatePixmap(dpy, win, screen_w, BAR_H, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_bar_buf_gc = XCreateGC(dpy, g_bar_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_bar_buf_gc);
    }
    unsigned long fg;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    gc = g_bar_buf_gc;
    XSetForeground(dpy, gc, bg_pixel);
    XFillRectangle(dpy, g_bar_buf, gc, 0, 0, screen_w, BAR_H);
    XSetForeground(dpy, gc, fg);
    win = g_bar_buf;
    XDrawLine(dpy, win, gc, 0, 0, screen_w, 0);

    /* REAL 2026-08-06, user: when toolbar has focus it owns arrows.
     * While Nav is active, [>] follows tab_focus_idx (keyboard cursor).
     * When Nav is idle, fall back to X focus / lowest-nav default. */
    int any_real_focus = 0;
    for (int i = 0; i < n_tabs; i++) {
        if (tab_has_focus(dpy, tabs[i].entity)) { any_real_focus = 1; break; }
    }
    int default_idx = -1;
    if (!any_real_focus && n_tabs > 0) {
        default_idx = 0;
        for (int i = 1; i < n_tabs; i++) {
            if (tabs[i].nav < tabs[default_idx].nav) default_idx = i;
        }
    }

    int close_x0 = screen_w - CLOSE_BTN_W;
    int shortcuts_x0 = close_x0 - n_shortcuts * SHORTCUT_W;
    int tabs_right = shortcuts_x0 - 4;
    if (tabs_right < TAB_W) tabs_right = screen_w / 2;

    for (int i = 0; i < n_tabs; i++) {
        int x0 = i * TAB_W;
        if (x0 + 8 >= tabs_right) break; /* stop before shortcuts / X */
        XDrawLine(dpy, win, gc, x0, 0, x0, BAR_H);
        int has_focus;
        if (nav_armed && n_tabs > 0) {
            int fi = tab_focus_idx;
            if (fi < 0) fi = 0;
            if (fi >= n_tabs) fi = n_tabs - 1;
            has_focus = (i == fi);
        } else {
            has_focus = tab_has_focus(dpy, tabs[i].entity) || i == default_idx;
        }
        const char *cursor = has_focus ? "[>]" : "[ ]";
        char label[192];
        /* Wraith/CHTPM style: [>] N. name  â€” number is jump address */
        snprintf(label, sizeof(label), "%s %d. %s", cursor, tabs[i].nav, tabs[i].entity);
        XDrawString(dpy, win, gc, x0 + 8, BAR_H / 2 + 4, label, (int)strlen(label));
    }
    /* Small status when right-click nav is armed (no typing box). */
    if (nav_armed) {
        const char *arm = digit_buf_len > 0 ? digit_buf : "NAV";
        char arm_lab[64];
        snprintf(arm_lab, sizeof(arm_lab), "[%s]", arm);
        XDrawString(dpy, win, gc, tabs_right - 80, BAR_H / 2 + 4, arm_lab, (int)strlen(arm_lab));
    }

    /* REAL, 2026-08-05, direct instruction ("i also want to add a 'x'
     * button to [the taskbar]. it will quit and save session"): a real
     * close button, far right of the bar. */
    XDrawRectangle(dpy, win, gc, close_x0, 2, CLOSE_BTN_W - 4, BAR_H - 5);
    XDrawString(dpy, win, gc, close_x0 + 14, BAR_H / 2 + 4, "X", 1);

    /* Real, generic shortcuts - drawn immediately left of the X button,
     * stacking further left as more real rows exist in
     * livedesk_shortcuts.pdl (see load_shortcuts()'s own header
     * comment). Each is a small real glyph square, own click region. */
    for (int i = 0; i < n_shortcuts; i++) {
        int sx0 = close_x0 - (i + 1) * SHORTCUT_W;
        XDrawRectangle(dpy, win, gc, sx0, 2, SHORTCUT_W - 4, BAR_H - 5);
        XDrawString(dpy, win, gc, sx0 + 8, BAR_H / 2 + 4, shortcuts[i].glyph, (int)strlen(shortcuts[i].glyph));
    }

    /* Single atomic copy onto the real, visible window - the only write
     * the window itself ever sees, so there's no visible gap between
     * "cleared" and "drawn" (see this function's own header comment). */
    XCopyArea(dpy, g_bar_buf, win_real, g_bar_buf_gc, 0, 0, screen_w, BAR_H, 0, 0);
}

/* REAL, 2026-08-05, direct instruction: "[X] will quit and save session
 * (ie desktop config however it was last and that will be what reloads
 * on open)." Rewrites $.crypts/autostart.pdl's own LAUNCH rows to match
 * exactly which entities are open right now (each relaunched via the
 * SAME real tp_desktop_window.+x <package_dir> invocation every entity
 * already uses, regardless of family - pet/asa-ava/monster all spawn
 * through this one real binary, confirmed via `ps aux` this session),
 * keeping the toolbar's own row too so it also comes back. Every real
 * MOUNT/STATE row is preserved unchanged - only LAUNCH rows are
 * replaced. Then closes every open entity for real (CLOSE via each
 * one's own interact_relay.txt - the same real relay command
 * tp_desktop_window.c already supports) and exits itself. */
static void quit_and_save_session(Display *dpy, const char *house_root, Tab *tabs, int n_tabs, const char *pid_path) {
    char pdl_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/$.crypts/autostart.pdl", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", pdl_path);

    char keep[64][PATH_BUF];
    int n_keep = 0;
    FILE *rf = fopen(pdl_path, "r");
    if (rf) {
        char line[PATH_BUF];
        while (n_keep < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "LAUNCH", 6) == 0) continue;
            snprintf(keep[n_keep], sizeof(keep[0]), "%s", line);
            n_keep++;
        }
        fclose(rf);
    }

    FILE *wf = fopen(tmp_path, "w");
    if (wf) {
        for (int i = 0; i < n_keep; i++) fputs(keep[i], wf);
        /* REAL FIX 2026-08-06, direct report ("script didn't open book
         * or ava&ava, only muchi-rancher"): house_root itself contains a
         * real, literal "&" (&.widgits/ is a real directory name in this
         * house) - an UNQUOTED "&" anywhere in a shell command string is
         * the real background-job operator, silently truncating
         * everything after it when crypt_autostart.c's system() call
         * parses this LAUNCH row. Every generated executable path must
         * be single-quoted, not just trailing args - this was the same
         * bug hiding in this file's own tool-bar row above (rc=0 looked
         * fine because system() itself doesn't fail, it just silently
         * ran the wrong, truncated command). */
        fprintf(wf, "LAUNCH       | tool-bar             | '%s/&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x' '%s'\n",
                house_root, house_root);
        for (int i = 0; i < n_tabs; i++) {
            fprintf(wf, "LAUNCH       | %-20s | '%s/&.widgits/tile-picker/ops/+x/tp_desktop_window.+x' '%s'\n",
                    tabs[i].entity, house_root, tabs[i].path);
        }
        fclose(wf);
        rename(tmp_path, pdl_path);
    }

    for (int i = 0; i < n_tabs; i++) {
        char relay[PATH_BUF];
        snprintf(relay, sizeof(relay), "%s/interact_relay.txt", tabs[i].path);
        FILE *cf = fopen(relay, "w");
        if (cf) { fprintf(cf, "CLOSE\n"); fclose(cf); }
    }

    (void)dpy;
    unlink(pid_path);
}


static void taskbar_set_wm_class(Display *dpy, Window w) {
    XClassHint *ch = XAllocClassHint();
    if (!ch) return;
    ch->res_name = (char *)"MuchiverseLivedesk";
    ch->res_class = (char *)"MuchiverseLivedesk";
    XSetClassHint(dpy, w, ch);
    XFree(ch);
}

/* Toolbar Nav (simple): no popup, no X grab. Soft-focus the bar and
 * use Left/Right/Up/Down for tab [>] only. Context menus keep the real
 * grab path on entities â€” toolbar never steals it. */
static void taskbar_soft_focus(Display *dpy, Window w) {
    if (!w) return;
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    XFlush(dpy);
}

/* Activate the tab under keyboard cursor: raise entity window (intentional
 * user action â€” not the old global focus-fight on every tab mouse click). */
static void taskbar_raise_tab(Display *dpy, Tab *tabs, int n_tabs, int idx) {
    if (idx < 0 || idx >= n_tabs) return;
    char target[160];
    snprintf(target, sizeof(target), "tile:%s-", tabs[idx].entity);
    Window w = find_by_name(dpy, RootWindow(dpy, DefaultScreen(dpy)), target);
    if (w) {
        XRaiseWindow(dpy, w);
        XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
        XFlush(dpy);
    }
}

/* Nav Enter / digit-jump: raise + open that entity's context menu
 * (OPEN_CONTEXT relay = same path as right-click on the tile). */
static void taskbar_activate_tab(Display *dpy, Tab *tabs, int n_tabs, int idx) {
    taskbar_raise_tab(dpy, tabs, n_tabs, idx);
    if (idx < 0 || idx >= n_tabs) return;
    if (tabs[idx].path[0]) {
        char relay[PATH_BUF];
        snprintf(relay, sizeof(relay), "%s/interact_relay.txt", tabs[idx].path);
        FILE *rf = fopen(relay, "w");
        if (rf) { fprintf(rf, "OPEN_CONTEXT\n"); fclose(rf); }
    }
}



int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_taskbar.+x <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/#.desktop/livedesk_taskbar.pid", house_root);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "cannot open display\n"); return 1; }
    /* Drop leftover grabs from older taskbar builds that held keyboard. */
    XUngrabKeyboard(dpy, CurrentTime);
    XUngrabPointer(dpy, CurrentTime);
    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));
    char theme_bg[32], theme_fg[32];
    load_theme(house_root, theme_bg, sizeof(theme_bg), theme_fg, sizeof(theme_fg));
    unsigned long bg_pixel = alloc_color_or(dpy, theme_bg, WhitePixel(dpy, DefaultScreen(dpy)));
    unsigned long fg_pixel = alloc_color_or(dpy, theme_fg, BlackPixel(dpy, DefaultScreen(dpy)));

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg_pixel;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                0, screen_h - BAR_H, screen_w, BAR_H, 0,
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    taskbar_set_wm_class(dpy, win);
    XMapRaised(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, fg_pixel);
    XSetBackground(dpy, gc, bg_pixel);

    /* REAL, 2026-08-05, direct instruction ("it should show active user
     * name on toolbar, as a higher layer on top right (doesn't need to
     * extend full length)"): a real, SEPARATE small override_redirect
     * window at the top-right of the screen (a genuinely different
     * layer from the bottom bar, matching "higher" - top of screen, not
     * bottom) - real active user via getenv("USER") (the real, standard
     * POSIX way, not invented), drawn once (static content, no need to
     * poll). */
    /* REAL FIX 2026-08-05, direct correction ("it shows user of linux
     * not user of livedesk (jb currently)"): getenv("USER") was the OS
     * login, not the real house-wide livedesk identity - that's tracked
     * by user-pal's own real login system (0.user-pal/00.login-signup/
     * current_login.txt, written by userpal_login.+x, read by its own
     * userpal_whoami.+x). Reused wholesale here, not reimplemented -
     * same real op every other user-pal caller already uses, just run
     * with PRISC_PROJECT_ROOT pointed at that piece's own root so it
     * finds the real current_login.txt. "none" (nobody logged in) falls
     * back to "guest", never to the unrelated OS username. */
    char username[128] = "guest";
    {
        char userpal_root[PATH_BUF];
        snprintf(userpal_root, sizeof(userpal_root), "%s/0.user-palðŸ‘¤ï¸/00.login-signup", house_root);
        char whoami_cmd[PATH_BUF * 2];
        snprintf(whoami_cmd, sizeof(whoami_cmd),
                 "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/userpal_whoami.+x' 2>/dev/null",
                 userpal_root, userpal_root);
        FILE *wp = popen(whoami_cmd, "r");
        if (wp) {
            char line[128];
            if (fgets(line, sizeof(line), wp)) {
                char *sp = strchr(line, ' ');
                if (sp) *sp = '\0';
                line[strcspn(line, "\r\n")] = '\0';
                if (line[0] && strcmp(line, "none") != 0) snprintf(username, sizeof(username), "%s", line);
            }
            pclose(wp);
        }
    }
    char user_label[160];
    snprintf(user_label, sizeof(user_label), "USER: %s", username);
    int user_w = (int)strlen(user_label) * 8 + 20;
    /* REAL FIX 2026-08-05, direct correction ("user should be right
     * above x, not at top of screen, near bottom"): moved from the
     * screen's top-right down to directly above the close button - a
     * real, separate override_redirect window (still its own "higher
     * layer" from the bottom bar, per the original instruction) stacked
     * immediately above CLOSE_BTN_W's own x0. */
    int user_x0 = screen_w - CLOSE_BTN_W - user_w;
    Window user_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                     user_x0, screen_h - BAR_H - BAR_H, user_w, BAR_H, 0,
                                     CopyFromParent, InputOutput, CopyFromParent,
                                     CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    taskbar_set_wm_class(dpy, user_win);
    XMapRaised(dpy, user_win);

    Shortcut shortcuts[MAX_SHORTCUTS];
    int n_shortcuts = load_shortcuts(house_root, shortcuts, MAX_SHORTCUTS);

    Tab tabs[MAX_TABS];
    int n_tabs = 0;
    int nav_armed = 0;       /* right-click arms arrows + # digit jump (wraith-style) */
    int tab_focus_idx = 0;
    char digit_buf[16] = "";  /* typed index while armed â€” no middle Nav box */
    int xfd = ConnectionNumber(dpy);
    int running = 1;

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        int need_redraw = 0;
        static struct timeval last_poll = { 0, 0 };
        struct timeval now;
        gettimeofday(&now, NULL);
        if (last_poll.tv_sec == 0 || (now.tv_sec - last_poll.tv_sec) >= 1) {
            n_tabs = load_tabs(house_root, tabs, MAX_TABS);
            sync_tab_claims(house_root, tabs, n_tabs);
            if (n_tabs <= 0) tab_focus_idx = 0;
            else if (tab_focus_idx >= n_tabs) tab_focus_idx = n_tabs - 1;
            need_redraw = 1;
            last_poll = now;
        }

        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (xev.type == Expose && xev.xany.window == user_win) {
                XClearArea(dpy, user_win, 0, 0, 0, 0, False);
                XDrawRectangle(dpy, user_win, gc, 0, 0, user_w - 1, BAR_H - 1);
                XDrawString(dpy, user_win, gc, 10, BAR_H / 2 + 4, user_label, (int)strlen(user_label));
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xany.window == win) {
                int close_x0 = screen_w - CLOSE_BTN_W;
                int shortcuts_x0 = close_x0 - n_shortcuts * SHORTCUT_W;
                int btn = xev.xbutton.button;

                if (xev.xbutton.x >= close_x0 && btn == 1) {
                    n_tabs = load_tabs(house_root, tabs, MAX_TABS);
                    quit_and_save_session(dpy, house_root, tabs, n_tabs, pid_path);
                    running = 0;
                } else if (btn == 1 && n_shortcuts > 0 && xev.xbutton.x >= shortcuts_x0 && xev.xbutton.x < close_x0) {
                    int sidx = (xev.xbutton.x - shortcuts_x0) / SHORTCUT_W;
                    if (sidx >= 0 && sidx < n_shortcuts) {
                        char cmd[PATH_BUF * 2];
                        snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", shortcuts[sidx].command);
                        int rc = system(cmd);
                        (void)rc;
                    }
                } else if (btn == 3) {
                    /* Right-click = arm toolbar nav (wraith-style engage).
                     * Arrows + digit-index jump. No middle typing box.
                     * No X grab â€” entity context menus stay free. */
                    nav_armed = 1;
                    digit_buf[0] = '\0';
                    taskbar_soft_focus(dpy, win);
                } else if (btn == 1) {
                    /* Left-click tab: mouse select. Disarm keyboard nav. */
                    nav_armed = 0;
                    digit_buf[0] = '\0';
                    int idx = xev.xbutton.x / TAB_W;
                    if (idx >= 0 && idx < n_tabs && (idx + 1) * TAB_W <= shortcuts_x0) {
                        tab_focus_idx = idx;
                        taskbar_raise_tab(dpy, tabs, n_tabs, idx);
                    }
                }
                need_redraw = 1;
            } else if (xev.type == KeyPress && nav_armed) {
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Left || ks == XK_Up) {
                    if (n_tabs > 0)
                        tab_focus_idx = (tab_focus_idx - 1 + n_tabs) % n_tabs;
                    digit_buf[0] = '\0'; /* chtpm: arrows reset digit_accum */
                } else if (ks == XK_Right || ks == XK_Down) {
                    if (n_tabs > 0)
                        tab_focus_idx = (tab_focus_idx + 1) % n_tabs;
                    digit_buf[0] = '\0';
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (digit_buf[0] == '\0') {
                        if (n_tabs > 0) {
                            if (tab_focus_idx < 0) tab_focus_idx = 0;
                            if (tab_focus_idx >= n_tabs) tab_focus_idx = n_tabs - 1;
                            taskbar_activate_tab(dpy, tabs, n_tabs, tab_focus_idx);
                        }
                    } else {
                        int nav_n = atoi(digit_buf);
                        if (nav_n > 0) {
                            char kind[8] = "", entity[128] = "", path[PATH_BUF] = "";
                            if (lookup_nav(house_root, nav_n, kind, sizeof(kind), entity, sizeof(entity), path, sizeof(path))) {
                                if (strcmp(kind, "tab") == 0) {
                                    for (int ti = 0; ti < n_tabs; ti++) {
                                        if (strcmp(tabs[ti].entity, entity) == 0) {
                                            tab_focus_idx = ti;
                                            taskbar_activate_tab(dpy, tabs, n_tabs, ti);
                                            break;
                                        }
                                    }
                                } else {
                                    char relay[PATH_BUF];
                                    snprintf(relay, sizeof(relay), "%s/interact_relay.txt", path);
                                    FILE *rf = fopen(relay, "w");
                                    if (rf) { fprintf(rf, "ACTIVATE_NAV:%d\n", nav_n); fclose(rf); }
                                }
                            }
                        }
                        digit_buf[0] = '\0';
                    }
                    nav_armed = 0; /* free keys for entity context after jump */
                } else if (ks == XK_Escape) {
                    digit_buf[0] = '\0';
                    nav_armed = 0;
                } else if (ks == XK_BackSpace) {
                    size_t l = strlen(digit_buf);
                    if (l > 0) digit_buf[l - 1] = '\0';
                } else if (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                    /* chtpm_parser_pal digit_accum: new_val = accum*10+d;
                     * accept only if 1..total_nav; else restart with d if
                     * d is valid alone. Cap length so we never grow forever. */
                    int d = kbuf[0] - '0';
                    int total_nav = max_claimed_nav(house_root);
                    if (total_nav < 1) total_nav = n_tabs > 0 ? n_tabs : 9;
                    int accum = atoi(digit_buf);
                    int new_val = accum * 10 + d;
                    if (new_val > 0 && new_val <= total_nav) {
                        snprintf(digit_buf, sizeof(digit_buf), "%d", new_val);
                    } else if (d > 0 && d <= total_nav) {
                        snprintf(digit_buf, sizeof(digit_buf), "%d", d);
                    } else if (d == 0 && accum == 0) {
                        /* leading zeros ignored */
                    } else {
                        /* out of bounds restart with bare digit if usable */
                        if (d > 0 && d <= total_nav)
                            snprintf(digit_buf, sizeof(digit_buf), "%d", d);
                        else
                            digit_buf[0] = '\0';
                    }
                    /* hard cap: never more digits than needed for total_nav */
                    {
                        int max_digits = 1, tn = total_nav;
                        while (tn >= 10) { max_digits++; tn /= 10; }
                        if ((int)strlen(digit_buf) > max_digits)
                            digit_buf[max_digits] = '\0';
                    }
                    /* chtpm do_jump: move [>] immediately to that index */
                    if (digit_buf[0]) {
                        int nav_n = atoi(digit_buf);
                        char kind[8] = "", entity[128] = "", path[PATH_BUF] = "";
                        if (lookup_nav(house_root, nav_n, kind, sizeof(kind), entity, sizeof(entity), path, sizeof(path))) {
                            if (strcmp(kind, "tab") == 0) {
                                for (int ti = 0; ti < n_tabs; ti++) {
                                    if (strcmp(tabs[ti].entity, entity) == 0) {
                                        tab_focus_idx = ti;
                                        break;
                                    }
                                }
                            } else if (strcmp(kind, "row") == 0 && path[0]) {
                                char relay[PATH_BUF];
                                snprintf(relay, sizeof(relay), "%s/interact_relay.txt", path);
                                FILE *rf = fopen(relay, "w");
                                if (rf) { fprintf(rf, "FOCUS_NAV:%d\n", nav_n); fclose(rf); }
                            }
                        }
                    }
                }
                need_redraw = 1;
            }
        }

        if (need_redraw) draw_bar(dpy, win, gc, screen_w, tabs, n_tabs, nav_armed, (int)strlen(digit_buf), shortcuts, n_shortcuts, bg_pixel, tab_focus_idx, digit_buf);
    }

    XCloseDisplay(dpy);
    unlink(pid_path);
    return 0;
}
