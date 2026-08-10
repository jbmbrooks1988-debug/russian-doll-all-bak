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
#include <X11/Xatom.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>


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
#define LOG_MAX_BYTES 262144  /* 256 KB limit per log file */

static void check_log_size(const char *log_path) {
    struct stat st;
    if (stat(log_path, &st) == 0 && st.st_size > LOG_MAX_BYTES) {
        truncate(log_path, 0);  /* Clear file if over 256 KB */
    }
}

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

#define HQ_BTN_W 40
#define HQ_POPUP_ROW_H 28
#define HQ_MENU_MAX 8
typedef struct {
    char label[64];
    char command[PATH_BUF];
    int nav; /* shared nav-claim number while this row's popup is open */
} HQMenuItem;

/* REAL, 2026-08-08, direct instruction ("top-left command strip: file /
 * desks / player / db / plugins"): the persistent top-left strip lives in
 * the same process as the taskbar (second strip, not a separate widget).
 * Each button is data-driven from #.desktop/livedesk_taskbar.pdl
 * (strip_btn_N_label / strip_btn_N_cmd / strip_btn_N_menu_M_label /
 * strip_btn_N_menu_M_cmd), falling back to documented defaults. */
#define STRIP_BTN_MAX 10
typedef struct {
    char label[64];
    char command[PATH_BUF];
    HQMenuItem menu[HQ_MENU_MAX];
    int n_menu;
} StripBtn;

/* One drawn cell of the strip. cells[0] is always the HQ button, cells[1]
 * the static user tag, cells[2..] the strip buttons. */
typedef struct {
    char label[64];
    HQMenuItem *menu;
    int n_menu;
    const char *cmd; /* button command when the button has no submenu */
    int is_static;   /* user tag: informational only, no click */
    int x0, x1;      /* click bounds, filled in by draw_strip */
    int nav;         /* live nav number, claimed by sync_strip_claims() */
    char sprite_path[PATH_BUF]; /* optional sprite.csv dir (user avatar cell) */
} StripCell;

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

/* REAL, 2026-08-09, DIRECT INSTRUCTION ("the black background behind all
 * the bars is supposed to be at 50% opacity. that should be modifiable
 * from the same .pdl/config as the colors/position"): same `COLOR | key |
 * value` row shape #.desktop/livedesk_theme.pdl already uses for bg/fg
 * (§A.7 house standard - runtime-tunable values belong in the nearest
 * existing .pdl, not hardcoded), a new `COLOR | opacity | 0.5` row.
 * Defaults to 0.5 (50%) when absent, matching the stated current
 * requirement, but is fully configurable without a recompile. Applied via
 * the standard `_NET_WM_WINDOW_OPACITY` property (see set_window_opacity()
 * below) - works under any compositor (picom/xcompmgr/etc), no ARGB visual
 * or window depth changes needed. */
static double load_theme_opacity(const char *house_root) {
    double opacity = 0.5;
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_theme.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return opacity;
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
        if (strcmp(key, "opacity") != 0) continue;

        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = '\0';
        char *v_end = v + strlen(v);
        while (v_end > v && v_end[-1] == ' ') v_end--;
        *v_end = '\0';
        if (v[0] == '\0') continue;
        double parsed = atof(v);
        if (parsed >= 0.0 && parsed <= 1.0) opacity = parsed;
    }
    fclose(f);
    return opacity;
}

/* Sets the real, standard EWMH window-opacity property a compositor
 * (picom/xcompmgr/etc) reads - CARDINAL/32, one value scaled to the full
 * 0..0xFFFFFFFF range. Silently a no-op with no visible effect if no
 * compositor is running, same as every other program that uses this
 * property - not something this file can detect or work around. */
static void set_window_opacity(Display *dpy, Window w, double opacity) {
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    Atom opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long val = (unsigned long)(opacity * (double)0xFFFFFFFFUL);
    XChangeProperty(dpy, w, opacity_atom, XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *)&val, 1);
}

/* REAL, 2026-08-08, direct instruction ("HQ button should be user
 * modifiable — if its not a file its a lie"): reads #.desktop/
 * livedesk_taskbar.pdl for HQ button label and menu items.
 * Missing keys fall back to safe defaults.
 *
 * REAL FIX 2026-08-08 ("restart shows up twice"): the menu was pre-filled
 * with $.restart/X.quit at index 0/1 BEFORE the .pdl rows were applied,
 * but the .pdl numbers its rows from 1 (hq_menu_1, hq_menu_2), so the
 * leftover default $.restart at index 0 leaked into the menu next to the
 * real row 1. Entries are now cleared first and only rows the .pdl
 * actually defines are shown; the defaults are used only when the .pdl
 * defines none.
 * A row with a label and an empty/absent command is a real "cancel" -
 * click/Enter just dismisses the popup (the dispatch below already
 * closes it for anything that isn't "quit" and isn't a shell command). */
static void load_hq_config(const char *house_root, char *hq_label, size_t hq_label_sz,
                           HQMenuItem *menu, int max_menu, int *n_menu) {
    snprintf(hq_label, hq_label_sz, "HQ");
    for (int i = 0; i < max_menu; i++) {
        menu[i].label[0] = '\0';
        menu[i].command[0] = '\0';
    }
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "SECTION", 7) != 0) continue;
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = strchr(p, '|');
            if (!end) continue;
            char *key_end = end;
            while (key_end > p && key_end[-1] == ' ') key_end--;
            char key[32];
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

            if (strcmp(key, "hq_label") == 0) {
                snprintf(hq_label, hq_label_sz, "%s", v);
            } else if (strncmp(key, "hq_menu_", 8) == 0) {
                int idx = atoi(key + 8);
                if (idx < 0 || idx >= max_menu) continue;
                if (strstr(key, "_label") != NULL) {
                    snprintf(menu[idx].label, sizeof(menu[idx].label), "%s", v);
                } else if (strstr(key, "_cmd") != NULL && v[0]) {
                    snprintf(menu[idx].command, sizeof(menu[idx].command), "%s", v);
                }
            }
        }
        fclose(f);
    }
    /* REAL FIX 2026-08-08 ("cancel button not showing in HQ"): the .pdl
     * numbers rows from 1 (hq_menu_1..N) so a full 1..3 file filled slots
     * 1,2,3, but the popup draws slots 0..n_menu-1 and n_menu counted only
     * the filled slots (3) - so row 3 (cancel) sat past the drawn range
     * and slot 0 stayed blank. Compact the filled 1-based slots down to
     * 0-based before counting; the defaults below are already 0-based. */
    int count = 0;
    for (int i = 0; i < max_menu; i++) {
        if (!menu[i].label[0]) continue;
        if (count != i) {
            snprintf(menu[count].label, sizeof(menu[count].label), "%s", menu[i].label);
            snprintf(menu[count].command, sizeof(menu[count].command), "%s", menu[i].command);
            menu[i].label[0] = '\0';
            menu[i].command[0] = '\0';
        }
        count++;
    }
    if (count > 0) {
        *n_menu = count;
        return;
    }
    /* Missing file or no menu rows: real documented defaults. */
    snprintf(menu[0].label, sizeof(menu[0].label), "$.restart");
    snprintf(menu[0].command, sizeof(menu[0].command), "%s", "setsid nohup $.crypts/button.sh run");
    snprintf(menu[1].label, sizeof(menu[1].label), "X.quit");
    snprintf(menu[1].command, sizeof(menu[1].command), "quit");
    snprintf(menu[2].label, sizeof(menu[2].label), "cancel");
    *n_menu = 3;
}

/* REAL, 2026-08-08, direct instruction ("runtime-configurable values are
 * always preferred over hardcoded constants"): reads #.desktop/
 * livedesk_taskbar.pdl for the top-left strip buttons. Same SECTION shape
 * as the HQ menu; missing keys fall back to the documented defaults.
 * A row with a label and an empty command is a real "cancel" - click/Enter
 * just dismisses the popup (see the dispatch in main). */
static void load_strip_config(const char *house_root, StripBtn *btns, int max_btns, int *n_btns,
                              int *strip_x_offset, int *strip_y_offset,
                              char *strip_user_cmd, size_t strip_user_cmd_sz) {
    *strip_x_offset = 0;
    /* REAL, 2026-08-09, DIRECT INSTRUCTION ("i like that position...leave
     * it there from now on" - the y=40 opacity-diagnostic offset from
     * opacity-bug-aug9.txt, moving the strip below GNOME Shell's own
     * native top panel): kept as the new real default, same
     * PDL-overridable shape as strip_x_offset (§A.7 house standard -
     * position belongs in the .pdl, not hardcoded). */
    *strip_y_offset = 40;
    strip_user_cmd[0] = '\0';
    /* Documented defaults: file (submenu), desks, player (submenu), db,
     * plugins. Commands intentionally left empty - the user fills them in
     * from the .pdl without recompiling. */
    *n_btns = 9;
    for (int i = 0; i < max_btns; i++) {
        btns[i].label[0] = '\0';
        btns[i].command[0] = '\0';
        btns[i].n_menu = 0;
        for (int j = 0; j < HQ_MENU_MAX; j++) {
            btns[i].menu[j].label[0] = '\0';
            btns[i].menu[j].command[0] = '\0';
        }
    }
    snprintf(btns[0].label, sizeof(btns[0].label), "file");
    btns[0].n_menu = 4;
    snprintf(btns[0].menu[0].label, sizeof(btns[0].menu[0].label), "new-desk");
    snprintf(btns[0].menu[0].command, sizeof(btns[0].menu[0].command), "livedesk:new-desk");
    snprintf(btns[0].menu[1].label, sizeof(btns[0].menu[1].label), "save");
    snprintf(btns[0].menu[1].command, sizeof(btns[0].menu[1].command), "livedesk:save");
    snprintf(btns[0].menu[2].label, sizeof(btns[0].menu[2].label), "save-as");
    snprintf(btns[0].menu[2].command, sizeof(btns[0].menu[2].command), "livedesk:save-as");
    snprintf(btns[0].menu[3].label, sizeof(btns[0].menu[3].label), "load");
    snprintf(btns[0].menu[3].command, sizeof(btns[0].menu[3].command), "livedesk:load");
    snprintf(btns[1].label, sizeof(btns[1].label), "desks");
    snprintf(btns[1].command, sizeof(btns[1].command), "livedesk:desks");
    snprintf(btns[2].label, sizeof(btns[2].label), "edit");
    snprintf(btns[3].label, sizeof(btns[3].label), "palettes");
    snprintf(btns[4].label, sizeof(btns[4].label), "player");
    btns[4].n_menu = 3;
    snprintf(btns[4].menu[0].label, sizeof(btns[4].menu[0].label), "play");
    snprintf(btns[4].menu[1].label, sizeof(btns[4].menu[1].label), "pause");
    snprintf(btns[4].menu[2].label, sizeof(btns[4].menu[2].label), "reset");
    snprintf(btns[5].label, sizeof(btns[5].label), "db");
    snprintf(btns[6].label, sizeof(btns[6].label), "plugins");
    snprintf(btns[7].label, sizeof(btns[7].label), "store");
    snprintf(btns[8].label, sizeof(btns[8].label), "network");

    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/livedesk_taskbar.pdl", house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SECTION", 7) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *key_end = end;
        while (key_end > p && key_end[-1] == ' ') key_end--;
        char key[48];
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

        if (strcmp(key, "strip_x_offset") == 0) {
            *strip_x_offset = atoi(v);
        } else if (strcmp(key, "strip_y_offset") == 0) {
            *strip_y_offset = atoi(v);
        } else if (strcmp(key, "strip_user_cmd") == 0) {
            snprintf(strip_user_cmd, strip_user_cmd_sz, "%s", v);
        } else if (strncmp(key, "strip_btn_", 10) != 0) {
            continue;
        } else {
            /* %n-verified full match: a trailing literal that fails (e.g.
             * sscanf("1_cmd","%d_label")==1) must NOT hit the _label branch -
             * only a pattern that consumes the WHOLE key after strip_btn_ wins. */
            int n = -1, m = -1, pos = 0;
            if (sscanf(key + 10, "%d_menu_%d_label%n", &n, &m, &pos) >= 2 &&
                key[10 + pos] == '\0' && n >= 0 && n < max_btns &&
                m >= 0 && m < HQ_MENU_MAX) {
                snprintf(btns[n].menu[m].label, sizeof(btns[n].menu[m].label), "%s", v);
                if (m + 1 > btns[n].n_menu) btns[n].n_menu = m + 1;
                if (n + 1 > *n_btns) *n_btns = n + 1;
            } else if (sscanf(key + 10, "%d_menu_%d_cmd%n", &n, &m, &pos) >= 2 &&
                       key[10 + pos] == '\0' && n >= 0 && n < max_btns &&
                       m >= 0 && m < HQ_MENU_MAX) {
                snprintf(btns[n].menu[m].command, sizeof(btns[n].menu[m].command), "%s", v);
                if (m + 1 > btns[n].n_menu) btns[n].n_menu = m + 1;
                if (n + 1 > *n_btns) *n_btns = n + 1;
            } else if (sscanf(key + 10, "%d_label%n", &n, &pos) >= 1 &&
                       key[10 + pos] == '\0' && n >= 0 && n < max_btns) {
                snprintf(btns[n].label, sizeof(btns[n].label), "%s", v);
                if (n + 1 > *n_btns) *n_btns = n + 1;
            } else if (sscanf(key + 10, "%d_cmd%n", &n, &pos) >= 1 &&
                       key[10 + pos] == '\0' && n >= 0 && n < max_btns) {
                snprintf(btns[n].command, sizeof(btns[n].command), "%s", v);
                if (n + 1 > *n_btns) *n_btns = n + 1;
            }
        }
    }
    fclose(f);
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
    int max_btn_nav = 0;  /* track max btn nav to start tabs after buttons */
    FILE *w = fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = fopen(claims_path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *navp = strstr(line, "NAV=");
            int nav_v = navp ? atoi(navp + 4) : 0;
            if (strncmp(line, "KIND=btn", 8) == 0) {
                /* Skip buttons when finding max_nav for tabs, but track their max */
                if (nav_v > max_btn_nav) max_btn_nav = nav_v;
                fputs(line, w);
                continue;
            }
            /* For tabs and rows, track max nav */
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
    /* Tabs start after buttons */
    int tab_start = max_btn_nav > 0 ? max_btn_nav : 7;  /* default 7 if no buttons yet */
    int next_nav = max_nav > tab_start ? max_nav : tab_start;
    for (int i = 0; i < n_tabs; i++) {
        if (tabs[i].nav == 0) {
            next_nav++;
            tabs[i].nav = next_nav;
            fprintf(w, "KIND=tab|PID=%d|NAV=%d|ENTITY=%s|PATH=%s\n",
                    tabs[i].pid, tabs[i].nav, tabs[i].entity, tabs[i].path);
        }
    }
    fclose(w);
    rename(tmp_path, claims_path);
}

/* REAL, 2026-08-08, DIRECT INSTRUCTION ("why none of them yet have nav
 * numbers" + "hq should have a nav number, and nav numbers grow"): the
 * persistent strip buttons claim real numbers from the SAME shared pool
 * the tabs claim from, tagged KIND=btn (taskbar-private namespace) so
 * sync_tab_claims and the popup claim/release helpers leave them alone.
 * Numbers are assigned left-to-right (HQ first) above the current pool
 * max, so they grow with every tab / menu-row claim that already exists,
 * and are refreshed every poll - stale KIND=btn rows from a dead taskbar
 * instance are dropped, tab claims and entity popup rows are untouched. */
static void sync_strip_claims(const char *house_root, StripCell *cells, int n_cells) {
    char claims_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_nav_claims.txt.tmp", house_root);
    int my_pid = (int)getpid();
    static int btn_nav_assigned = 0;  /* remember if we've already assigned nav numbers */
    for (int i = 0; i < n_cells; i++) cells[i].nav = 0;
    FILE *w = fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = fopen(claims_path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            /* Strip buttons always use fixed nav 1..n_cells, drop all button entries */
            if (strncmp(line, "KIND=btn", 8) == 0) {
                continue;  /* Will be rewritten below with fixed nav 1..n_cells */
            }
            /* Keep tab and row entries as-is */
            fputs(line, w);
        }
        fclose(f);
    }
    /* Always assign FIXED nav numbers 1..n_cells (never changes for strip buttons) */
    for (int i = 0; i < n_cells; i++) {
        cells[i].nav = i + 1;  /* 1, 2, 3, ... for HQ, user, file, etc. */
        fprintf(w, "KIND=btn|PID=%d|NAV=%d|PATH=%s\n", my_pid, cells[i].nav, house_root);
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
        snprintf(kind_out, kind_sz, "%s",
                 strncmp(line, "KIND=tab", 8) == 0 ? "tab" :
                 strncmp(line, "KIND=btn", 8) == 0 ? "btn" : "row");
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

/* REAL, 2026-08-08, HQ nav-index gap (direct instruction: wire HQ + its
 * submenu rows into the shared live nav-claim pool, §D.2): while a popup
 * is open, each of its rows claims a real number from the SAME pool
 * entity context menus claim from (KIND=row). Numbers are assigned in
 * row order at open time, released when the popup closes, and never
 * collide with tab claims (sync_tab_claims only manages KIND=tab lines
 * and leaves our rows alone). Same read-prune-write-rename pattern as
 * sync_tab_claims - the taskbar is the only writer of its own PID rows. */
static void popup_claim(const char *house_root, int pid, HQMenuItem *menu, int n_menu) {
    char claims_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_nav_claims.txt.tmp", house_root);
    FILE *w = fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = fopen(claims_path, "r");
    int max_nav = 0;
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *navp = strstr(line, "NAV=");
            int v = navp ? atoi(navp + 4) : 0;
            if (v > max_nav) max_nav = v;
            char *pidp = strstr(line, "PID=");
            int pv = pidp ? atoi(pidp + 4) : -1;
            if (pv != pid || strncmp(line, "KIND=row", 8) != 0)
                fputs(line, w); /* drop only our own open-menu rows, keep btn/tab claims */
        }
        fclose(f);
    }
    for (int i = 0; i < n_menu; i++) {
        max_nav++;
        menu[i].nav = max_nav;
        fprintf(w, "KIND=row|PID=%d|NAV=%d|PATH=%s\n", pid, max_nav, house_root);
    }
    fclose(w);
    rename(tmp_path, claims_path);
}

static void popup_release(const char *house_root, int pid) {
    char claims_path[PATH_BUF], tmp_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    snprintf(tmp_path, sizeof(tmp_path), "%s/#.desktop/livedesk_nav_claims.txt.tmp", house_root);
    FILE *w = fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = fopen(claims_path, "r");
    if (f) {
        char line[PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *pidp = strstr(line, "PID=");
            int pv = pidp ? atoi(pidp + 4) : -1;
            if (pv != pid || strncmp(line, "KIND=row", 8) != 0)
                fputs(line, w); /* popup rows only - btn/tab claims stay */
        }
        fclose(f);
    }
    fclose(w);
    rename(tmp_path, claims_path);
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

/* Check if a remote entity (not this taskbar process) has an open context menu.
 * Used to yield keyboard focus to entity windows when their menus are active. */
static int remote_entity_menu_open(const char *house_root) {
    int my_pid = (int)getpid();
    char claims_path[PATH_BUF];
    snprintf(claims_path, sizeof(claims_path), "%s/#.desktop/livedesk_nav_claims.txt", house_root);
    FILE *f = fopen(claims_path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* Look for KIND=row entries (context menu rows) with different PID */
        if (strncmp(line, "KIND=row", 8) != 0) continue;
        char *pidp = strstr(line, "PID=");
        if (!pidp) continue;
        int pid_v = atoi(pidp + 4);
        if (pid_v > 0 && pid_v != my_pid) {
            /* Found a context menu row owned by a different process */
            found = 1;
            break;
        }
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

/* ========================================================================
 * 2026-08-08 TASK 3: tiny deskpal sprite on each toolbar tab. Same
 * sprite.csv format tp_desktop_window.c's own load_sprite_csv reads
 * (# resolution=N, then N*N "r,g,b,a" rows) - the entity's sprite lives
 * at <tab path>/sprite.csv (e.g. the book-stack entity's own sprite.csv). No GL here (the bar is plain X11): each sprite is cached
 * once per path, composited against the bar's solid bg pixel (RGBA alpha
 * over the theme background) and XPutImage'd straight into the
 * double-buffer pixmap before draw_bar's single atomic XCopyArea.
 * Missing/unreadable sprite = text-only fallback, never a crash.
 * ======================================================================== */
#define TAB_SPRITE_PX 24
typedef struct {
    char path[PATH_BUF];
    unsigned char *rgba; /* res * res * 4 */
    int res;
} TabSprite;
static TabSprite g_sprite_cache[MAX_TABS];

static TabSprite *tab_sprite(const char *path) {
    if (!path || !path[0]) return NULL;
    for (int i = 0; i < MAX_TABS; i++) {
        if (g_sprite_cache[i].rgba && strcmp(g_sprite_cache[i].path, path) == 0) return &g_sprite_cache[i];
    }
    char csv_path[PATH_BUF];
    snprintf(csv_path, sizeof(csv_path), "%s/sprite.csv", path);
    FILE *f = fopen(csv_path, "r");
    if (!f) return NULL;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return NULL; }
    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return NULL; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int rr, gg, bb, aa;
        if (sscanf(line, "%d,%d,%d,%d", &rr, &gg, &bb, &aa) == 4) {
            pixels[count * 4 + 0] = (unsigned char)rr;
            pixels[count * 4 + 1] = (unsigned char)gg;
            pixels[count * 4 + 2] = (unsigned char)bb;
            pixels[count * 4 + 3] = (unsigned char)aa;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return NULL; }
    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_sprite_cache[i].rgba) {
            snprintf(g_sprite_cache[i].path, sizeof(g_sprite_cache[i].path), "%s", path);
            g_sprite_cache[i].rgba = pixels;
            g_sprite_cache[i].res = res;
            return &g_sprite_cache[i];
        }
    }
    free(pixels);
    return NULL;
}

static void blit_tab_sprite(Display *dpy, Drawable d, GC gc, TabSprite *sp,
                            int x0, int y0, int px, unsigned long bg_pixel) {
    Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    unsigned long rmask = vis->red_mask, gmask = vis->green_mask, bmask = vis->blue_mask;
    int rshift = 0, gshift = 0, bshift = 0;
    while (rmask && !(rmask & (1UL << rshift))) rshift++;
    while (gmask && !(gmask & (1UL << gshift))) gshift++;
    while (bmask && !(bmask & (1UL << bshift))) bshift++;
    unsigned long br = (bg_pixel >> rshift) & 0xff;
    unsigned long bg2 = (bg_pixel >> gshift) & 0xff;
    unsigned long bb = (bg_pixel >> bshift) & 0xff;
    int res = sp->res;
    unsigned char *buf = calloc((size_t)px * px, 4);
    if (!buf) return;
    for (int y = 0; y < px; y++) {
        int sy = (y * res) / px;
        if (sy >= res) sy = res - 1;
        for (int x = 0; x < px; x++) {
            int sx = (x * res) / px;
            if (sx >= res) sx = res - 1;
            const unsigned char *pix = &sp->rgba[(sy * res + sx) * 4];
            int a = pix[3];
            int r = (pix[0] * a + (int)br * (255 - a)) / 255;
            int g = (pix[1] * a + (int)bg2 * (255 - a)) / 255;
            int b = (pix[2] * a + (int)bb * (255 - a)) / 255;
            unsigned long word = ((unsigned long)r << rshift) | ((unsigned long)g << gshift) | ((unsigned long)b << bshift);
            buf[(y * px + x) * 4 + 0] = (unsigned char)(word & 0xff);
            buf[(y * px + x) * 4 + 1] = (unsigned char)((word >> 8) & 0xff);
            buf[(y * px + x) * 4 + 2] = (unsigned char)((word >> 16) & 0xff);
            buf[(y * px + x) * 4 + 3] = (unsigned char)((word >> 24) & 0xff);
        }
    }
    XImage *img = XCreateImage(dpy, vis, depth, ZPixmap, 0, (char *)buf, px, px, 32, 0);
    if (img) {
        img->byte_order = LSBFirst; /* buf[] is written LSB-first above */
        XPutImage(dpy, d, gc, img, 0, 0, x0, y0, px, px);
        XDestroyImage(img);
    } else {
        free(buf);
    }
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
                     const char *hq_label, unsigned long bg_pixel, int tab_focus_idx,
                     const char *digit_buf, int strip_focus_cell) {
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

    /* REAL, 2026-08-09, DIRECT INSTRUCTION ("nav should ALWAYS default to
     * 1... get rid of the one that starts at 8"): the old "lowest-nav
     * tab" default_idx fallback below used to give some tab a [>] cursor
     * whenever no tab had real X input focus - true at every program
     * startup, since nothing has real X focus yet. That's exactly why nav
     * 8 always showed a phantom cursor even with the strip's own button 1
     * now defaulting to focused (strip_focus_cell=0 from program start,
     * see its own declaration). The header owns default priority focus
     * now, unconditionally - no other widget gets its own competing
     * "nothing else has focus so I'll grab it" fallback anymore. Only a
     * tab with genuine real X input focus (an actual window the user
     * clicked into) may ever show its own cursor outside of armed-mode
     * nav. */

    /* REAL, 2026-08-08, HQ moved up into the top-left strip (see main):
     * nothing is drawn at the right edge of the bottom bar anymore. */
    int tabs_right = screen_w - 8;

    for (int i = 0; i < n_tabs; i++) {
        int x0 = i * TAB_W;
        if (x0 + 8 >= tabs_right) break; /* stop before the right margin */
        XDrawLine(dpy, win, gc, x0, 0, x0, BAR_H);
        int has_focus;
        if (nav_armed && strip_focus_cell >= 0) {
            /* Armed, but the SINGLE unified cursor (see nav_focus_apply())
             * is currently on a strip button, not any tab - no tab may show
             * a cursor at all right now. Falling through to the "not armed"
             * branch below here was the real bug: it drew its OWN
             * independent real-X-focus/default-lowest-nav cursor on tab 0
             * (nav=8) regardless of nav_armed, producing two live [>]
             * cursors on screen at once (button 1 AND tab 8). */
            has_focus = 0;
        } else if (nav_armed && n_tabs > 0) {
            /* Armed AND the unified cursor has moved onto the tab range. */
            int fi = tab_focus_idx;
            if (fi < 0) fi = 0;
            if (fi >= n_tabs) fi = n_tabs - 1;
            has_focus = (i == fi);
        } else {
            has_focus = tab_has_focus(dpy, tabs[i].entity);
        }
        const char *cursor = has_focus ? "[>]" : "[ ]";
        char label[192];
        /* Wraith/CHTPM style: [>] N. name  â€” number is jump address */
        snprintf(label, sizeof(label), "%s %d. %s", cursor, tabs[i].nav, tabs[i].entity);
        /* REAL 2026-08-08 TASK 3: tiny deskpal sprite on the left edge of
         * the tab, before the text. Missing/unreadable sprite.csv falls
         * back to text-only (text stays at the old x0+8). */
        TabSprite *sp = tab_sprite(tabs[i].path);
        int text_x = x0 + 8;
        if (sp) {
            int sy = (BAR_H - TAB_SPRITE_PX) / 2;
            blit_tab_sprite(dpy, win, gc, sp, x0 + 2, sy, TAB_SPRITE_PX, bg_pixel);
            text_x = x0 + 8 + TAB_SPRITE_PX;
        }
        XDrawString(dpy, win, gc, text_x, BAR_H / 2 + 4, label, (int)strlen(label));
    }
    /* REAL, 2026-08-09, DIRECT INSTRUCTION: the "[NAV]"/"[<digits>]" armed
     * indicator moved to draw_strip() (left of button 1 / HQ) - no longer
     * drawn here. digit_buf_len/digit_buf params kept for signature
     * stability at the one call site, now unused in this function. */
    (void)digit_buf_len; (void)digit_buf;

    /* Single atomic copy onto the real, visible window - the only write
     * the window itself ever sees, so there's no visible gap between
     * "cleared" and "drawn" (see this function's own header comment). */
    XCopyArea(dpy, g_bar_buf, win_real, g_bar_buf_gc, 0, 0, screen_w, BAR_H, 0, 0);
}

/* REAL, 2026-08-05, direct instruction: "[X] will quit and save session
 * (ie desktop config however it was last and that will be what reloads
 * on open)." The session-close part is real; the destructive rewrite
 * turned out to be wrong for this desk because it erased the curated
 * startup list and collapsed later resets. Keep the close behavior, but
 * do not rewrite $.crypts/autostart.pdl here. */
static void quit_and_save_session(Display *dpy, const char *house_root, Tab *tabs, int n_tabs, const char *pid_path) {
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


/* REAL, 2026-08-08, direct instruction ("HQ button opens a popup with
 * $.restart and X.quit like windows"): tiny override_redirect popup,
 * no grabs (avoids XWayland issues), dismisses on outside-click or
 * Escape. */
static void clamp_popup_to_screen(Display *dpy, int *x, int *y, int w, int h) {
    int scr = DefaultScreen(dpy);
    int sw = DisplayWidth(dpy, scr);
    int sh = DisplayHeight(dpy, scr);
    const int margin = 4;
    const int taskbar_reserve = 40;
    int usable_h = sh - taskbar_reserve;
    if (usable_h < h + margin) usable_h = sh - margin;
    int px = *x, py = *y;
    if (px + w + margin > sw) px = sw - w - margin;
    if (px < margin) px = margin;
    if (py + h + margin > usable_h) py = usable_h - h - margin;
    if (py < margin) py = margin;
    *x = px;
    *y = py;
}

static char g_house_root[PATH_BUF] = "."; /* set once in main() - lets draw_hq_popup() below write its own frame-history log without threading house_root through every call site */

static void draw_hq_popup(Display *dpy, Window popup, GC gc, HQMenuItem *menu, int n_menu, int focus_row);

static Window open_hq_popup(Display *dpy, GC gc, const char *house_root, int root_x, int root_y,
                            HQMenuItem *menu, int n_menu) {
    int h = HQ_POPUP_ROW_H * n_menu;
    int w = 180;
    clamp_popup_to_screen(dpy, &root_x, &root_y, w, h);
    char bg_name[32] = "white";
    char fg_name[32] = "black";
    load_theme(house_root, bg_name, sizeof(bg_name), fg_name, sizeof(fg_name));
    unsigned long bg_pixel = alloc_color_or(dpy, bg_name, WhitePixel(dpy, DefaultScreen(dpy)));
    unsigned long fg_pixel = alloc_color_or(dpy, fg_name, BlackPixel(dpy, DefaultScreen(dpy)));
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg_pixel;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                 root_x, root_y, (unsigned)w, (unsigned)h, 1,
                                 CopyFromParent, InputOutput, CopyFromParent,
                                 CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XClassHint *class_hint = XAllocClassHint();
    if (class_hint) {
        class_hint->res_name = (char *)"MuchiverseLivedesk";
        class_hint->res_class = (char *)"MuchiverseLivedesk";
        XSetClassHint(dpy, popup, class_hint);
        XFree(class_hint);
    }
    XMapRaised(dpy, popup);
    XSetForeground(dpy, gc, fg_pixel);
    XSync(dpy, False);
    /* REAL FIX 2026-08-08, HQ popup text invisible: draw the menu text
     * right here with the SAME pipeline the taskbar itself uses (shared
     * gc + XDrawString, see draw_bar) instead of waiting on Expose - the
     * generic `xev.type == Expose` branch below was swallowing the
     * popup's Expose, so draw_hq_popup() never ran and only the black
     * background ever showed. Expose redraws (occlusion etc.) are still
     * handled in the main loop below. */
    draw_hq_popup(dpy, popup, gc, menu, n_menu, 0);
    XEvent stale_ev;
    while (XCheckWindowEvent(dpy, popup, ButtonPressMask | KeyPressMask, &stale_ev)) {
    }
    return popup;
}

static void draw_hq_popup(Display *dpy, Window popup, GC gc, HQMenuItem *menu, int n_menu, int focus_row) {
    XGCValues saved;
    XGetGCValues(dpy, gc, GCForeground | GCFont, &saved);
    XClearWindow(dpy, popup);
    int h = HQ_POPUP_ROW_H * n_menu;
    int w = 180;
    XDrawRectangle(dpy, popup, gc, 0, 0, w - 1, h - 1);
    for (int i = 0; i < n_menu; i++) {
        int row_y = i * HQ_POPUP_ROW_H;
        if (i > 0) XDrawLine(dpy, popup, gc, 0, row_y, w, row_y);
        const char *cursor = (i == focus_row) ? "[>]" : "[ ]";
        char labeled[160];
        /* REAL, 2026-08-09, DIRECT INSTRUCTION ("sub indexes are the only
         * ones using nav [while a popup is open]... start numbering over
         * within sub till canceled"): the row's real number here is its
         * LOCAL position (i+1, always 1..n_menu) - what a human sitting in
         * front of this popup actually types - not menu[i].nav, the huge
         * GLOBAL claim number from the shared pool (kept, unchanged, for
         * the separate cross-window ACTIVATE_NAV/lookup_nav mechanism -
         * see popup_claim()'s own header comment). Nothing outside this
         * popup can navigate while it's open anyway, so the global number
         * was never meaningful to type here in the first place. */
        if (menu[i].nav > 0)
            snprintf(labeled, sizeof(labeled), "%s %d. %s", cursor, i + 1, menu[i].label);
        else
            snprintf(labeled, sizeof(labeled), "%s %s", cursor, menu[i].label);
        XDrawString(dpy, popup, gc, 12, row_y + HQ_POPUP_ROW_H / 2 + 4, labeled, (int)strlen(labeled));
    }
    XSetForeground(dpy, gc, saved.foreground);

    /* REAL, 2026-08-09, DIRECT INSTRUCTION ("also there should be frame
     * history"): same real-RGB-receipt + per-frame draw log already built
     * for the strip (strip_frame.raw/.receipt.txt/strip_frame_log.txt),
     * now covering popup windows (HQ menu + strip submenus) too - both go
     * through this one function, so adding it here covers every popup
     * call site automatically. Every draw_hq_popup() call is already
     * event-driven (only called when something genuinely changed - arrow
     * key, digit press, initial open), so no separate marker-file throttle
     * is needed here the way draw_strip_if_marked() needed one for the
     * once-a-second strip poll. */
    {
        char dbg_dir[PATH_BUF], raw_path[PATH_BUF], receipt_path[PATH_BUF], log_path[PATH_BUF];
        snprintf(dbg_dir, sizeof(dbg_dir), "%s/#.desktop/tp_taskbar_debug", g_house_root);
        mkdir(dbg_dir, 0755);
        snprintf(raw_path, sizeof(raw_path), "%s/popup_frame.raw", dbg_dir);
        snprintf(receipt_path, sizeof(receipt_path), "%s/popup_frame.receipt.txt", dbg_dir);
        snprintf(log_path, sizeof(log_path), "%s/popup_frame_log.txt", dbg_dir);

        XImage *img = XGetImage(dpy, popup, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
        if (img) {
            unsigned char *rgba = malloc((size_t)w * h * 4);
            if (rgba) {
                for (int py = 0; py < h; py++) {
                    for (int px = 0; px < w; px++) {
                        unsigned long p = XGetPixel(img, px, py);
                        size_t o = ((size_t)py * w + px) * 4;
                        rgba[o + 0] = (unsigned char)((p >> 16) & 0xFF);
                        rgba[o + 1] = (unsigned char)((p >> 8) & 0xFF);
                        rgba[o + 2] = (unsigned char)(p & 0xFF);
                        rgba[o + 3] = 255;
                    }
                }
                FILE *rf = fopen(raw_path, "wb");
                if (rf) { fwrite(rgba, 1, (size_t)w * h * 4, rf); fclose(rf); }
                free(rgba);
            }
            XDestroyImage(img);
        }
        FILE *rc = fopen(receipt_path, "w");
        if (rc) { fprintf(rc, "frame_w=%d\nframe_h=%d\n", w, h); fclose(rc); }

        check_log_size(log_path);
        FILE *lf = fopen(log_path, "a");
        if (lf) {
            time_t t = time(NULL);
            fprintf(lf, "--- popup frame @ %ld (focus_row=%d, n_menu=%d) ---\n", (long)t, focus_row, n_menu);
            for (int i = 0; i < n_menu; i++) {
                fprintf(lf, "  row[%d] local_nav=%d label=\"%s\" global_nav=%d focused=%d\n",
                        i, i + 1, menu[i].label, menu[i].nav, i == focus_row);
            }
            fclose(lf);
        }
    }
}

static void close_hq_popup(Display *dpy, Window popup) {
    if (popup) XDestroyWindow(dpy, popup);
}

/* ========================================================================
 * 2026-08-08 TOP-LEFT COMMAND STRIP (2do task #4). A persistent second
 * strip pinned at the top-left of the screen, living in this same process
 * so it opens when the taskbar opens and stays open. cells[0] = HQ button
 * (leaves its old right-edge spot), cells[1] = the user/guest tag (leaves
 * the top-middle-right, DIRECT INSTRUCTION: before "file"), cells[2..] =
 * file / desks / player / db / plugins. Buttons with a submenu open the
 * same popup HQ uses; buttons without one run their config command.
 * ======================================================================== */
#define STRIP_PAD 6

/* Popup + strip state - file scope so the event loop and helpers agree.
 * g_focus_restore_win is the window keyboard focus returns to when a
 * popup closes (the bottom bar), so armed-mode nav keeps receiving keys. */
static Window g_hq_popup = 0;
static int g_hq_popup_open = 0;
static int g_hq_focus_row = -1;
static int g_hq_digit_accum = 0;
static Window g_strip_popup = 0;
static int g_strip_popup_open = 0;
static int g_strip_popup_focus = -1;
static int g_strip_popup_digit_accum = 0;
static int g_strip_popup_n = 0;
static HQMenuItem *g_strip_popup_menu = NULL;
static int g_livedesk_popup_x = 0; /* anchor: strip cell that opened the last livedesk popup */
static char g_file_label[96] = "";  /* current file-button text, e.g. "file:pre-design" */
static int g_strip_x_offset = 0;
static int g_strip_y_offset = 0; /* strip's own real screen y - popups below it must add this, see open_cell_popup() */
static Window g_focus_restore_win = 0;
/* REAL, 2026-08-09, DIRECT INSTRUCTION ("it should put > on 1 before right
 * clicking. it should happen on start automatically... nav should ALWAYS
 * default to 1... see chtpm_parser"): chtpm_parser_pal.c's own main() does
 * `focus_index = 0` unconditionally at startup, before any input has
 * happened - nav focus is never "nothing" there, it's always index 0 by
 * default. Mirrored here: strip_focus_cell defaults to 0 (button 1 / HQ),
 * not -1, so the very first draw_strip() call already shows [>] on it -
 * no right-click/arm gesture required for the cursor to exist. */
static int strip_focus_cell = 0;

/* REAL, 2026-08-09, DIRECT INSTRUCTION ("both toolbars should be on the
 * same khtpm/chtpm layout" / "header should be same and get priority [>]
 * focus"): mirrors chtpm_parser_pal.c's own real focus mechanism exactly -
 * a SINGLE `focus_index` spanning every navigable element as one list
 * (that file's own do-while-skip-non-navigable loop, e.g. line ~3308:
 * `do { focus_index++; if (focus_index >= element_count) focus_index = 0; }
 * while (focus_index != prev && !is_navigable(focus_index));`), not two
 * separately-tracked cursor variables per widget. The earlier "two
 * cursors" bug (button 1 AND tab 8 both showing [>] at once) was a direct
 * consequence of tracking strip_focus_cell and tab_focus_idx as two
 * independent variables that could desync - patching draw_bar() to
 * cross-check strip_focus_cell was a symptom fix, not the real one.
 *
 * The unified list here is [strip buttons 0..n_cells-1][tabs 0..n_tabs-1]
 * - strip buttons occupy the LOW indices, so they are not "hardcoded" to
 * lose to the bottom bar; the bottom bar was never structurally
 * prioritized either way before this - it just drew its own independent
 * cursor whenever nav_armed was true, regardless of where the real focus
 * actually was. Arming always starts focus_index at 0 (the first strip
 * button, i.e. the header), matching the direct instruction that the
 * header should get priority [>] focus on arm. strip_focus_cell/
 * tab_focus_idx remain as DERIVED display variables (draw_strip/draw_bar
 * still read them directly) but must only ever be set together, via
 * nav_focus_apply() below - never independently again. */
static int nav_is_navigable(StripCell *cells, int n_cells, int n_tabs, int idx) {
    if (idx < 0) return 0;
    if (idx < n_cells) return !cells[idx].is_static;
    return idx < n_cells + n_tabs;
}

static void nav_focus_step(StripCell *cells, int n_cells, int n_tabs, int *focus, int delta) {
    int total = n_cells + n_tabs;
    if (total <= 0) { *focus = -1; return; }
    if (*focus < 0) *focus = (delta > 0) ? -1 : 0; /* so a +1 step from "none" lands on 0 */
    int prev = *focus;
    do {
        *focus += delta;
        if (*focus < 0) *focus = total - 1;
        if (*focus >= total) *focus = 0;
    } while (*focus != prev && !nav_is_navigable(cells, n_cells, n_tabs, *focus));
}

/* Single place that ever writes strip_focus_cell/tab_focus_idx from a
 * unified focus_index - keeps the two derived variables from ever
 * disagreeing about which one thing is actually focused. */
static void nav_focus_apply(int focus, int n_cells, int *strip_focus_cell_out, int *tab_focus_idx_out) {
    if (focus >= 0 && focus < n_cells) {
        *strip_focus_cell_out = focus;
        *tab_focus_idx_out = -1;
    } else if (focus >= n_cells) {
        *strip_focus_cell_out = -1;
        *tab_focus_idx_out = focus - n_cells;
    } else {
        *strip_focus_cell_out = -1;
        *tab_focus_idx_out = -1;
    }
}

/* The exact string draw_strip paints for one cell - shared with main()'s
 * strip-window width calc so buttons never outgrow their window once nav
 * numbers appear. */
static void format_strip_cell(StripCell *c, int focused, int show_cursor, char *out, size_t sz) {
    if (c->nav > 0) {
        snprintf(out, sz, "%s %d. %s", focused ? "[>]" : "[ ]", c->nav, c->label);
    } else {
        snprintf(out, sz, "%s", c->label);
    }
}

/* REAL, 2026-08-09, DIRECT INSTRUCTION ("the [NAV] var currently on bottom
 * toolbar... we should move it up... to be left of 1.HQ"): the armed-mode
 * "[NAV]"/"[<digits>]" indicator used to be drawn by draw_bar() near the
 * bottom bar's right edge - moved here instead, always reserved as fixed
 * left padding before cell 0 (HQ) so the strip's own width never has to
 * shift/resize depending on whether it's currently shown. */
#define STRIP_NAV_BOX_W 64

static Pixmap g_strip_buf = 0;
static GC g_strip_buf_gc = 0;

/* REAL BUG FIX, 2026-08-09 (direct instruction: "the bottom has the
 * opacity but not the top" - traced, not guessed): draw_bar() has always
 * rendered into an offscreen Pixmap and done ONE XCopyArea onto the real
 * window (see g_bar_buf above); draw_strip() instead drew straight to the
 * real window via XClearWindow every frame - the one genuine structural
 * difference between the two windows at the X11 level, even though both
 * had the identical _NET_WM_WINDOW_OPACITY property set (verified via
 * `xprop`) in the identical create/map/set-opacity order. Converted to the
 * exact same offscreen-pixmap-then-XCopyArea pattern as draw_bar so both
 * bars are structurally identical to whatever compositor is reading them -
 * removes the only place a compositor-specific difference could hide. */
static void draw_strip(Display *dpy, Window win, GC gc, int win_w,
                       StripCell *cells, int n_cells,
                       unsigned long bg_pixel, int show_cursor, const char *digit_buf) {
    Window win_real = win;
    if (!g_strip_buf) {
        g_strip_buf = XCreatePixmap(dpy, win, win_w, BAR_H, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_strip_buf_gc = XCreateGC(dpy, g_strip_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_strip_buf_gc);
    }
    unsigned long fg;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    gc = g_strip_buf_gc;
    XSetForeground(dpy, gc, bg_pixel);
    XFillRectangle(dpy, g_strip_buf, gc, 0, 0, win_w, BAR_H);
    XSetForeground(dpy, gc, fg);
    win = g_strip_buf;
    XDrawLine(dpy, win, gc, 0, 0, win_w, 0);
    if (show_cursor) {
        const char *arm = (digit_buf && digit_buf[0]) ? digit_buf : "NAV";
        char arm_lab[64];
        snprintf(arm_lab, sizeof(arm_lab), "[%s]", arm);
        XDrawString(dpy, win, gc, 4, BAR_H / 2 + 4, arm_lab, (int)strlen(arm_lab));
    }
    int x = STRIP_NAV_BOX_W;
    for (int i = 0; i < n_cells; i++) {
        char display[160];
        /* Hide toolbar [>] when a popup is open - unifies visual focus to popup only */
        int is_focused = (i == strip_focus_cell) && !g_hq_popup_open && !g_strip_popup_open;
        format_strip_cell(&cells[i], is_focused, show_cursor, display, sizeof(display));
        TabSprite *sp = cells[i].sprite_path[0] ? tab_sprite(cells[i].sprite_path) : NULL;
        int w = (int)strlen(display) * 8 + 20;
        if (sp) w += TAB_SPRITE_PX + 6;
        if (w < 40) w = 40;
        cells[i].x0 = x;
        cells[i].x1 = x + w;
        int tx = x;
        if (sp) {
            blit_tab_sprite(dpy, win, gc, sp, x + 4, (BAR_H - TAB_SPRITE_PX) / 2, TAB_SPRITE_PX, bg_pixel);
            tx = x + TAB_SPRITE_PX + 10;
        }
        if (cells[i].is_static) {
            XDrawString(dpy, win, gc, tx + 4, BAR_H / 2 + 4, display, (int)strlen(display));
        } else {
            XDrawRectangle(dpy, win, gc, x, 2, w - 4, BAR_H - 5);
            XDrawString(dpy, win, gc, tx + 8, BAR_H / 2 + 4, display, (int)strlen(display));
        }
        x += w + STRIP_PAD;
    }
    XCopyArea(dpy, g_strip_buf, win_real, g_strip_buf_gc, 0, 0, win_w, BAR_H, 0, 0);
}

/* REAL, 2026-08-09, DIRECT INSTRUCTION ("we create a log of everything that
 * gets drawn, everytime frame changes...we only render from frame change...
 * where we draw to frame before rendering and input key first to history
 * file and use marker file to render"): ported the SAME marker-driven
 * render contract real piececraft-xyz's chtpm_parser_pal.c main() already
 * uses (its own block comment: "RENDER TRIGGER - MARKER-DRIVEN, SINGLE
 * SOURCE OF TRUTH... compose_frame() ONLY fires when frame_changed.txt
 * grows... The marker file IS the throttle") - not a new invention. Three
 * pieces, same shape as that precedent:
 *   1. append_key_history() - every armed-mode keypress is appended to
 *      #.desktop/tp_taskbar_debug/key_history.txt FIRST, before any state
 *      changes (mirrors keyboard_input.c's append_key() writing
 *      pieces/keyboard/history.txt before process_key() ever runs).
 *   2. mark_strip_frame_changed() - an APPEND-ONLY marker file
 *      (strip_frame_changed.txt); every place that mutates what the strip
 *      actually shows (strip_focus_cell, tab_focus_idx, nav_armed,
 *      digit_buf, or cells[].nav after a claims sync) appends one line.
 *   3. draw_strip_if_marked() - the only render entrypoint main() should
 *      call now: stats the marker file, and ONLY redraws + recaptures when
 *      its size grew since the last check - exactly compose_frame()'s own
 *      "ONLY fires when frame_changed.txt grows" rule, replacing the old
 *      unconditional per-poll draw_strip() call.
 * The RGB receipt itself (strip_frame.raw + .receipt.txt) still uses the
 * SEPARATE 014.wsr-pal compose_rgb_frame.c/dump_rgb_png.c raw+receipt
 * contract (frame_w/frame_h keys) - captured via XGetImage right after the
 * real draw_strip() call, i.e. the TRUE rendered pixels, not a
 * reimplemented rasterizer (unlike compose_rgb_frame.c's own CPU
 * rasterizer, which only exists because that project's GL path has no
 * text-drawing API at all - tp_taskbar already draws through real Xlib, so
 * reading back what Xlib actually painted is strictly simpler and more
 * accurate than re-deriving it). */
static long g_last_strip_marker_size = -1;

static void mark_strip_frame_changed(const char *house_root, const char *reason) {
    char dbg_dir[PATH_BUF], marker_path[PATH_BUF];
    snprintf(dbg_dir, sizeof(dbg_dir), "%s/#.desktop/tp_taskbar_debug", house_root);
    mkdir(dbg_dir, 0755);
    snprintf(marker_path, sizeof(marker_path), "%s/strip_frame_changed.txt", dbg_dir);
    check_log_size(marker_path);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fprintf(mf, "K %s\n", reason); fclose(mf); }
}

/* Appends the raw key BEFORE it's acted on - same "history file gets the
 * input first" ordering as keyboard_input.c's append_key(). ks_name is a
 * short human label (e.g. "Left", "Digit:3", "Return") since KeySym values
 * alone aren't self-describing in a log a human is meant to read. */
static void append_key_history(const char *house_root, const char *ks_name,
                                int strip_focus_cell_before, int tab_focus_idx_before) {
    char dbg_dir[PATH_BUF], hist_path[PATH_BUF];
    snprintf(dbg_dir, sizeof(dbg_dir), "%s/#.desktop/tp_taskbar_debug", house_root);
    mkdir(dbg_dir, 0755);
    snprintf(hist_path, sizeof(hist_path), "%s/key_history.txt", dbg_dir);
    check_log_size(hist_path);
    FILE *hf = fopen(hist_path, "a");
    if (hf) {
        time_t t = time(NULL);
        fprintf(hf, "KEY_PRESSED: %s @ %ld (strip_focus_cell=%d, tab_focus_idx=%d)\n",
                ks_name, (long)t, strip_focus_cell_before, tab_focus_idx_before);
        fclose(hf);
    }
}

static void capture_strip_frame(Display *dpy, Window win, const char *house_root,
                                 int win_w, int win_h,
                                 StripCell *cells, int n_cells, int show_cursor) {
    char dbg_dir[PATH_BUF], raw_path[PATH_BUF], receipt_path[PATH_BUF], log_path[PATH_BUF];
    snprintf(dbg_dir, sizeof(dbg_dir), "%s/#.desktop/tp_taskbar_debug", house_root);
    mkdir(dbg_dir, 0755);
    snprintf(raw_path, sizeof(raw_path), "%s/strip_frame.raw", dbg_dir);
    snprintf(receipt_path, sizeof(receipt_path), "%s/strip_frame.receipt.txt", dbg_dir);
    snprintf(log_path, sizeof(log_path), "%s/strip_frame_log.txt", dbg_dir);

    XImage *img = XGetImage(dpy, win, 0, 0, (unsigned)win_w, (unsigned)win_h, AllPlanes, ZPixmap);
    if (img) {
        unsigned char *rgba = malloc((size_t)win_w * win_h * 4);
        if (rgba) {
            for (int py = 0; py < win_h; py++) {
                for (int px = 0; px < win_w; px++) {
                    unsigned long p = XGetPixel(img, px, py);
                    size_t o = ((size_t)py * win_w + px) * 4;
                    rgba[o + 0] = (unsigned char)((p >> 16) & 0xFF);
                    rgba[o + 1] = (unsigned char)((p >> 8) & 0xFF);
                    rgba[o + 2] = (unsigned char)(p & 0xFF);
                    rgba[o + 3] = 255;
                }
            }
            FILE *rf = fopen(raw_path, "wb");
            if (rf) { fwrite(rgba, 1, (size_t)win_w * win_h * 4, rf); fclose(rf); }
            free(rgba);
        }
        XDestroyImage(img);
    }
    FILE *rc = fopen(receipt_path, "w");
    if (rc) {
        fprintf(rc, "frame_w=%d\nframe_h=%d\n", win_w, win_h);
        fclose(rc);
    }
    check_log_size(log_path);
    FILE *lf = fopen(log_path, "a");
    if (lf) {
        time_t t = time(NULL);
        fprintf(lf, "--- frame @ %ld (armed=%d, strip_focus_cell=%d, n_cells=%d) ---\n",
                (long)t, show_cursor, strip_focus_cell, n_cells);
        for (int i = 0; i < n_cells; i++) {
            fprintf(lf, "  cell[%d] label=\"%s\" nav=%d focused=%d\n",
                    i, cells[i].label, cells[i].nav, i == strip_focus_cell);
        }
        fclose(lf);
    }
}

/* The ONLY render entrypoint main() should call for the strip window now -
 * mirrors chtpm_parser_pal.c main()'s own frame_changed.txt stat-and-
 * compare loop exactly: redraw (and recapture the RGB receipt) ONLY when
 * the marker file's size grew since the last check. Every caller that used
 * to invoke draw_strip() unconditionally now calls this instead. */
static void draw_strip_if_marked(Display *dpy, Window win, GC gc, const char *house_root,
                                  int win_w, int win_h,
                                  StripCell *cells, int n_cells,
                                  unsigned long bg_pixel, int show_cursor, const char *digit_buf) {
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/#.desktop/tp_taskbar_debug/strip_frame_changed.txt", house_root);
    struct stat st;
    long cur_size = (stat(marker_path, &st) == 0) ? (long)st.st_size : 0;
    if (g_last_strip_marker_size >= 0 && cur_size == g_last_strip_marker_size) return; /* marker didn't grow - skip draw + capture */
    draw_strip(dpy, win, gc, win_w, cells, n_cells, bg_pixel, show_cursor, digit_buf);
    capture_strip_frame(dpy, win, house_root, win_w, win_h, cells, n_cells, show_cursor);
    g_last_strip_marker_size = cur_size;
}

/* Close whichever popup is open (HQ or a strip submenu) and release its
 * shared nav-claim rows back into the pool. */
static void close_popups(Display *dpy, const char *house_root) {
    if (g_hq_popup_open) {
        close_hq_popup(dpy, g_hq_popup);
        popup_release(house_root, (int)getpid());
        g_hq_popup = 0;
        g_hq_popup_open = 0;
        g_hq_focus_row = -1;
        g_hq_digit_accum = 0;
    }
    if (g_strip_popup_open) {
        close_hq_popup(dpy, g_strip_popup);
        popup_release(house_root, (int)getpid());
        g_strip_popup = 0;
        g_strip_popup_open = 0;
        g_strip_popup_focus = -1;
        g_strip_popup_digit_accum = 0;
        g_strip_popup_n = 0;
        g_strip_popup_menu = NULL;
    }
    if (g_focus_restore_win) {
        XSetInputFocus(dpy, g_focus_restore_win, RevertToParent, CurrentTime);
        XFlush(dpy);
    }
}

static int cell_for_nav(StripCell *cells, int n_cells, int nav_n) {
    for (int i = 0; i < n_cells; i++) if (cells[i].nav == nav_n) return i;
    return -1;
}

/* REAL, 2026-08-09, DIRECT INSTRUCTION ("[submenu numbers] start in way
 * out numbers... stupid and inaccessible, since sub indexes are the only
 * ones using nav [while the popup is open]... start numbering over within
 * sub till canceled, leave external numbering [unchanged]"): digits
 * accumulate (new_val = accum*10+d) and move [>] within a LOCAL 1..n_menu
 * range - what a human sitting in front of an open popup actually types -
 * instead of the previous [menu[0].nav, menu[n-1].nav] GLOBAL claim range
 * (which could be e.g. 15..18 depending on how much else the shared pool
 * had already claimed, unreachable by typing "1"). menu[i].nav itself is
 * untouched - still claimed from the shared pool by popup_claim() below,
 * still what lookup_nav()/remote ACTIVATE_NAV addressing use for jumping
 * into this popup from OUTSIDE it - only the LOCAL typing/display path
 * changes. Nothing outside an open popup is navigable while it's open
 * anyway, so the global number was never meaningful to type here. */
static int popup_digit(HQMenuItem *menu, int n, int *accum, int *focus, int d) {
    if (n <= 0) return 0;
    int lo = 1, hi = n;
    int new_val = *accum * 10 + d;
    if (new_val >= lo && new_val <= hi) {
        *accum = new_val;
        *focus = new_val - lo;
        return 1;
    } else if (d >= lo && d <= hi) {
        *accum = d;
        *focus = d - lo;
        return 1;
    }
    *accum = 0;
    return 0;
}

/* LIVEDESK SESSIONS + DESKS (K9/K10) - self-contained pure logic
 * (definitions live after active_avatar_dir(), before main()). Reserved
 * "livedesk:*" commands dispatch IN-PROCESS (no system() shell-out);
 * everything else still fires via setsid nohup as before. */
static void livedesk_dispatch(Display *dpy, GC gc, const char *house_root, const char *cmd);

/* Open the given strip cell the same way a click would: claim its submenu
 * rows from the shared pool, pop the popup BELOW the strip at the strip's
 * real screen x (strip_x_offset + cell x - popups are RootWindow children),
 * and hand keyboard focus to the popup so its rows are actually navigable.
 * A cell with no submenu runs its own command instead. */
static void open_cell_popup(Display *dpy, GC gc, const char *house_root,
                            StripCell *cells, int n_cells, int idx,
                            HQMenuItem *hq_menu, int hq_n_menu) {
    if (idx < 0 || idx >= n_cells) return;
    close_popups(dpy, house_root);
    if (cells[idx].n_menu > 0) {
        int px = g_strip_x_offset + cells[idx].x0;
        int py = g_strip_y_offset + BAR_H;
        popup_claim(house_root, (int)getpid(), cells[idx].menu, cells[idx].n_menu);
        if (cells[idx].menu == hq_menu) {
            g_hq_popup = open_hq_popup(dpy, gc, house_root, px, py, hq_menu, hq_n_menu);
            g_hq_popup_open = 1;
            g_hq_focus_row = 0;
            g_hq_digit_accum = 0;
        } else {
            g_strip_popup = open_hq_popup(dpy, gc, house_root, px, py, cells[idx].menu, cells[idx].n_menu);
            g_strip_popup_open = 1;
            g_strip_popup_focus = 0;
            g_strip_popup_digit_accum = 0;
            g_strip_popup_n = cells[idx].n_menu;
            g_strip_popup_menu = cells[idx].menu;
        }
        /* REAL, 2026-08-09, DIRECT INSTRUCTION ("do it the way i
         * suggested, the proven way"): use the SAME taskbar_soft_focus()
         * (XRaiseWindow -> XSetInputFocus -> XFlush) already empirically
         * proven this session to deliver real XTest-injected KeyPress
         * events to the strip window - confirmed via key_injector +
         * strip_frame_log.txt showing strip_focus_cell actually moving.
         * The popup's own focus code previously called bare
         * XSetInputFocus with no XRaiseWindow first - XGetInputFocus
         * still reported success (confirmed via [DEBUG FOCUS] logging),
         * yet injected keys never arrived at all (confirmed via zero
         * [DEBUG HQ-POPUP KEY] output) - i.e. XSetInputFocus alone is
         * NOT reliable for a brand-new override-redirect window under
         * this Mutter/XWayland environment, but the raise-then-focus
         * sequence already proven for strip_win is. Reusing the proven
         * mechanism instead of introducing something new
         * (XGrabKeyboard) that has no working precedent in this file. */
        if (g_hq_popup_open) {
            taskbar_soft_focus(dpy, g_hq_popup);
        } else if (g_strip_popup_open) {
            taskbar_soft_focus(dpy, g_strip_popup);
        }
    } else if (cells[idx].cmd && cells[idx].cmd[0]) {
        if (strncmp(cells[idx].cmd, "livedesk:", 9) == 0) {
            g_livedesk_popup_x = g_strip_x_offset + cells[idx].x0; /* anchor the dynamic popup under THIS cell, like file's submenu */
            livedesk_dispatch(dpy, gc, house_root, cells[idx].cmd);
        } else {
            char cmd[PATH_BUF * 2];
            snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", cells[idx].cmd);
            int rc = system(cmd);
            (void)rc;
        }
    }
}

/* Run one popup row: "quit" closes the session (real, 2026-08-05 behavior),
 * "livedesk:*" dispatches in-process (sessions/desks, 2026-08-09), any other
 * non-empty command is fired via setsid nohup from house_root (the process
 * chdir'd there at startup, see §F-18 - never resolve from the inherited
 * CWD). A label-only row is a real "cancel". */
static void run_popup_row(Display *dpy, GC gc, const char *house_root, Tab *tabs, int *n_tabs,
                          const char *pid_path, HQMenuItem *menu, int row, int *running) {
    if (strcmp(menu[row].command, "quit") == 0) {
        *n_tabs = load_tabs(house_root, tabs, MAX_TABS);
        quit_and_save_session(dpy, house_root, tabs, *n_tabs, pid_path);
        *running = 0;
    } else if (strncmp(menu[row].command, "livedesk:", 9) == 0) {
        livedesk_dispatch(dpy, gc, house_root, menu[row].command);
    } else if (menu[row].command[0]) {
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", menu[row].command);
        int rc = system(cmd);
        (void)rc;
    }
}

/* REAL, 2026-08-09, DIRECT INSTRUCTION ("they should be using basically
 * the exact same code"): win and strip_win used to be two separate
 * XCreateWindow/set_wm_class/XMapRaised/set_window_opacity call sites that
 * happened to match property-for-property (verified via `xprop` - every
 * property was byte-identical) but were free to silently drift apart in
 * the future since nothing forced them to stay in sync. One shared
 * function now builds BOTH bar windows, so "identical setup" is
 * structurally guaranteed by construction, not by two humans/agents
 * carefully copy-pasting the same five calls twice. */
static Window create_bar_window(Display *dpy, XSetWindowAttributes *swa,
                                 int x, int y, int w, int h, double opacity) {
    Window bw = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                               x, y, w, h, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWEventMask, swa);
    taskbar_set_wm_class(dpy, bw);
    XMapRaised(dpy, bw);
    set_window_opacity(dpy, bw, opacity);
    return bw;
}

/* Read one value for `key` from a kv or pdl file: "key=value" lines or
 * "SECTION | key | value" rows both work (the last | segment is the value). */
static void read_key_value(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        if (strncmp(line, "SECTION", 7) == 0 || strncmp(line, "META", 4) == 0) continue;
        char *p = strstr(line, key);
        if (!p) continue;
        char *eq = strchr(p, '=');
        char *bar = strrchr(p, '|');
        char *v = NULL;
        if (eq && (!bar || eq < bar)) v = eq + 1;
        else if (bar) v = bar + 1;
        if (!v) continue;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        size_t n = strlen(v);
        while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = '\0';
        if (v[0]) { snprintf(out, out_sz, "%s", v); break; }
    }
    fclose(f);
}

/* Resolve the logged-in user's active avatar sprite dir under the house
 * root: <house_root>/xyzfs/users/<user_uuid>/home/avatars/<avatar_uuid>.
 * Empty when no login or no active avatar (text-only USER cell). */
static void active_avatar_dir(const char *house_root, char *out, size_t out_sz) {
    out[0] = '\0';
    char login_root[PATH_BUF];
    snprintf(login_root, sizeof(login_root), "%s/0.user-pal👤️/00.login-signup", house_root);
    char login_path[PATH_BUF], sess_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
    snprintf(sess_path, sizeof(sess_path), "%s/xyzfs/session.pdl", login_root);
    char user_uuid[128] = "", avatar_uuid[128] = "";
    read_key_value(login_path, "current_user_uuid", user_uuid, sizeof(user_uuid));
    read_key_value(sess_path, "active_avatar_uuid", avatar_uuid, sizeof(avatar_uuid));
    if (!user_uuid[0] || !avatar_uuid[0]) return;
    snprintf(out, out_sz, "%s/xyzfs/users/%s/home/avatars/%s", house_root, user_uuid, avatar_uuid);
}

/* ========================================================================
 * LIVEDESK SESSIONS + DESKS (K9/K10, 2026-08-09) - self-contained pure
 * logic per Q5 (stay LEGACY, port to khtpm_core only later).
 *
 * Storage (design §4, Q3): <house>/xyzfs/users/<uuid>/home/livedesk/
 * sessions/ - a FULL copy per session (Q1):
 *   sessions/session.pdl          STATE | active_session | <id>
 *                                 STATE | last_session   | <id>
 *   sessions/<id>/session.pdl     STATE | name | <display>  STATE | active_desk | desk_0N
 *   sessions/<id>/desks/<desk>.pdl  DESK | entity | path | x | y | grid_x | grid_y | glyph | index
 *   sessions/<id>/entities/         (C5: FULL per-session entity data, next pass)
 *
 * Desk switch = snapshot outgoing desk (live positions from livedesk_open.txt
 * + desktop_pos.txt) -> CLOSE all entities via their relay -> spawn the
 * incoming desk's rows via tp_desktop_window.+x (each row's desktop_pos.txt
 * is rewritten so the entity starts exactly at its saved grid cell).
 * ======================================================================== */
#define LIVEDESK_GRID_PX 80   /* matches GRID_CELL_PX in tp_desktop_window.c */
#define LIVEDESK_DYN_MAX 24
#define LIVEDESK_MAX_OPEN 64

static HQMenuItem g_livedesk_dyn[LIVEDESK_DYN_MAX];

/* mkdir every missing ancestor level (plain mkdir() creates one level only
 * and the per-user <home>/livedesk/sessions chain rarely exists yet). */
static void livedesk_mkdir_p(const char *path) {
    char tmp[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

/* Session desk pdl rows must be HOUSE-RELATIVE (portable save files), not
 * absolute machine paths - snapshot strips the house_root prefix and
 * spawn re-joins it. Registry (livedesk_open.txt) stays absolute since
 * that's live runtime state owned by the entity windows themselves. */
static void livedesk_rel_path(const char *house_root, const char *abs, char *out, size_t sz) {
    size_t hl = strlen(house_root);
    if (strncmp(abs, house_root, hl) == 0 && abs[hl] == '/')
        snprintf(out, sz, "%s", abs + hl + 1);
    else
        snprintf(out, sz, "%s", abs);
}

static void livedesk_join_path(const char *house_root, const char *rel, char *out, size_t sz) {
    if (rel[0] == '/')
        snprintf(out, sz, "%s", rel);
    else
        snprintf(out, sz, "%s/%s", house_root, rel);
}

/* Emoji-free upward walk to the account registry (new-code rule: never
 * hand-write emoji into source - read names from disk). */
static int livedesk_login_root(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    DIR *d = opendir(house_root);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "0.user-pal", 10) == 0) {
            snprintf(out, sz, "%s/%s/00.login-signup", house_root, e->d_name);
            break;
        }
    }
    closedir(d);
    return out[0] != '\0';
}

static void livedesk_user_uuid(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char login_root[PATH_BUF];
    if (!livedesk_login_root(house_root, login_root, sizeof(login_root))) return;
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/current_login.txt", login_root);
    read_key_value(p, "current_user_uuid", out, sz);
}

/* 1 = per-user sessions root exists (logged-in user), else 0. */
static int livedesk_sessions_root(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char uuid[128] = "";
    livedesk_user_uuid(house_root, uuid, sizeof(uuid));
    if (!uuid[0]) return 0;
    snprintf(out, sz, "%s/xyzfs/users/%s/home/livedesk/sessions", house_root, uuid);
    return 1;
}

static void livedesk_root_read(const char *sroot, char *active, size_t asz, char *last, size_t lsz) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/session.pdl", sroot);
    if (active) read_key_value(p, "active_session", active, asz);
    if (last) read_key_value(p, "last_session", last, lsz);
}

/* Read-only display name of the active session (for the file button label).
 * Never creates state - if no active session exists yet the caller shows
 * the plain "file" label. */
static int livedesk_current_session_name(const char *house_root, char *out, size_t sz) {
    out[0] = '\0';
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char active[64] = "";
    livedesk_root_read(sroot, active, sizeof(active), NULL, 0);
    if (!active[0]) return 0;
    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sroot, active);
    char name[256] = "";
    read_key_value(sp, "name", name, sizeof(name));
    if (name[0]) snprintf(out, sz, "%s", name);
    else snprintf(out, sz, "%s", active);
    return 1;
}

static void livedesk_root_write(const char *sroot, const char *active, const char *last) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/session.pdl", sroot);
    livedesk_mkdir_p(sroot);
    char keep[8][PATH_BUF];
    int n_keep = 0;
    FILE *f = fopen(p, "r");
    if (f) {
        char line[PATH_BUF];
        while (n_keep < 8 && fgets(line, sizeof(line), f)) {
            if (strstr(line, "active_session") || strstr(line, "last_session")) continue;
            snprintf(keep[n_keep], sizeof(keep[n_keep]), "%s", line);
            n_keep++;
        }
        fclose(f);
    }
    FILE *w = fopen(p, "w");
    if (!w) return;
    for (int i = 0; i < n_keep; i++) fputs(keep[i], w);
    if (active && active[0]) fprintf(w, "STATE | active_session | %s\n", active);
    if (last && last[0]) fprintf(w, "STATE | last_session | %s\n", last);
    fclose(w);
}

static void livedesk_session_dir(const char *sroot, const char *id, char *out, size_t sz) {
    snprintf(out, sz, "%s/%s", sroot, id);
}

static int livedesk_next_id(const char *sroot, char *out, size_t sz) {
    int best = 0;
    DIR *d = opendir(sroot);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == 's' && isdigit((unsigned char)e->d_name[1])) {
                int n = atoi(e->d_name + 1);
                if (n > best) best = n;
            }
        }
        closedir(d);
    }
    snprintf(out, sz, "s%d", best + 1);
    return best + 1;
}

static void livedesk_set_name(const char *sroot, const char *id, const char *name) {
    char sdir[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    char desk[64] = "";
    read_key_value(sp, "active_desk", desk, sizeof(desk));
    FILE *f = fopen(sp, "w");
    if (!f) return;
    fprintf(f, "STATE | name | %s\n", name);
    if (desk[0]) fprintf(f, "STATE | active_desk | %s\n", desk);
    fclose(f);
}

/* Create <sessions>/<id> with session.pdl(name) + desks/ + an empty
 * desk_01.pdl if the id is brand new; never overwrites an existing one. */
static void livedesk_ensure_session(const char *sroot, const char *id, const char *name) {
    char sdir[PATH_BUF], desks[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    livedesk_mkdir_p(sdir);
    livedesk_mkdir_p(desks);
    char sp[PATH_BUF];
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    char existing[256] = "";
    read_key_value(sp, "name", existing, sizeof(existing));
    if (!existing[0]) {
        FILE *f = fopen(sp, "w");
        if (f) { fprintf(f, "STATE | name | %s\n", name); fclose(f); }
    }
    char d1[PATH_BUF];
    snprintf(d1, sizeof(d1), "%s/desk_01.pdl", desks);
    if (access(d1, F_OK) != 0) {
        FILE *f = fopen(d1, "w");
        if (f) fclose(f);
    }
}

static void livedesk_session_name(const char *sroot, const char *id, char *out, size_t sz) {
    out[0] = '\0';
    char sdir[PATH_BUF], sp[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    read_key_value(sp, "name", out, sz);
    if (!out[0]) snprintf(out, sz, "%s", id);
}

static void livedesk_active_desk(const char *sroot, const char *id, char *out, size_t sz) {
    out[0] = '\0';
    char sdir[PATH_BUF], sp[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    read_key_value(sp, "active_desk", out, sz);
    if (!out[0]) snprintf(out, sz, "desk_01");
}

static void livedesk_write_active_desk(const char *sroot, const char *id, const char *desk) {
    char sdir[PATH_BUF], sp[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(sp, sizeof(sp), "%s/session.pdl", sdir);
    char name[256] = "";
    read_key_value(sp, "name", name, sizeof(name));
    FILE *f = fopen(sp, "w");
    if (!f) return;
    if (name[0]) fprintf(f, "STATE | name | %s\n", name);
    fprintf(f, "STATE | active_desk | %s\n", desk);
    fclose(f);
}

static int livedesk_desk_list(const char *sroot, const char *id, char out[][64], int max) {
    char sdir[PATH_BUF], desks[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    DIR *d = opendir(desks);
    int n = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (n >= max) break;
            const char *dot = strrchr(e->d_name, '.');
            if (dot && strcmp(dot, ".pdl") == 0 && dot != e->d_name) {
                size_t len = (size_t)(dot - e->d_name);
                if (len >= 64) len = 63;
                memcpy(out[n], e->d_name, len);
                out[n][len] = '\0';
                n++;
            }
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(out[j], out[i]) < 0) {
                    char t[64];
                    snprintf(t, sizeof(t), "%s", out[i]);
                    snprintf(out[i], sizeof(out[i]), "%s", out[j]);
                    snprintf(out[j], sizeof(out[j]), "%s", t);
                }
    }
    return n;
}

static int livedesk_next_desk(const char *sroot, const char *id, char *out, size_t sz) {
    char sdir[PATH_BUF], desks[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    int best = 0;
    DIR *d = opendir(desks);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "desk_", 5) == 0 && isdigit((unsigned char)e->d_name[5])) {
                int n = atoi(e->d_name + 5);
                if (n > best) best = n;
            }
        }
        closedir(d);
    }
    snprintf(out, sz, "desk_%02d", best + 1);
    return best + 1;
}

/* Live registry snapshot: PID / INDEX / ENTITY / PATH per alive row. */
static int livedesk_read_open(const char *house_root, int *pids, char ents[][128],
                              char paths[][PATH_BUF], int *indexes, int max) {
    char reg[PATH_BUF];
    snprintf(reg, sizeof(reg), "%s/#.desktop/livedesk_open.txt", house_root);
    registry_lock_acquire(house_root);
    FILE *f = fopen(reg, "r");
    if (!f) { registry_lock_release(); return 0; }
    int n = 0;
    char line[PATH_BUF];
    while (n < max && fgets(line, sizeof(line), f)) {
        int pid = 0, idx = 0;
        char ent[128] = "", path[PATH_BUF] = "";
        char *p;
        if ((p = strstr(line, "PID="))) pid = atoi(p + 4);
        if ((p = strstr(line, "INDEX="))) idx = atoi(p + 6);
        if ((p = strstr(line, "ENTITY="))) {
            char *e = p + 7, *end = strchr(e, '|');
            size_t len = end ? (size_t)(end - e) : strcspn(e, "\r\n");
            if (len >= sizeof(ent)) len = sizeof(ent) - 1;
            memcpy(ent, e, len);
            ent[len] = '\0';
        }
        if ((p = strstr(line, "PATH="))) {
            snprintf(path, sizeof(path), "%s", p + 5);
            path[strcspn(path, "\r\n")] = '\0';
        }
        if (!ent[0] || !pid_is_alive(pid)) continue;
        if (pids) pids[n] = pid;
        if (indexes) indexes[n] = idx;
        snprintf(ents[n], 128, "%s", ent);
        snprintf(paths[n], PATH_BUF, "%s", path);
        n++;
    }
    fclose(f);
    registry_lock_release();
    return n;
}

static void livedesk_glyph(const char *package_dir, char *out, size_t sz) {
    out[0] = '\0';
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/glyph.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return;
    if (fgets(out, (int)sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int livedesk_read_pos(const char *package_dir, int *x, int *y) {
    *x = -1;
    *y = -1;
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) *x = atoi(line + 2);
        else if (strncmp(line, "y=", 2) == 0) *y = atoi(line + 2);
    }
    fclose(f);
    return (*x >= 0 && *y >= 0);
}

/* Write the live desktop (registry + desktop_pos.txt) into the session's
 * active desk pdl. This is the Q9 "File->save" / auto-save-on-switch. */
static void livedesk_snapshot_desk(const char *house_root, const char *sroot, const char *id) {
    char active[64] = "";
    livedesk_active_desk(sroot, id, active, sizeof(active));
    char sdir[PATH_BUF], desks[PATH_BUF], sp[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    livedesk_mkdir_p(sdir);
    livedesk_mkdir_p(desks);
    snprintf(sp, sizeof(sp), "%s/%s.pdl", desks, active);
    int pids[LIVEDESK_MAX_OPEN], indexes[LIVEDESK_MAX_OPEN];
    char ents[LIVEDESK_MAX_OPEN][128], paths[LIVEDESK_MAX_OPEN][PATH_BUF];
    int n = livedesk_read_open(house_root, pids, ents, paths, indexes, LIVEDESK_MAX_OPEN);
    FILE *w = fopen(sp, "w");
    if (!w) return;
    for (int i = 0; i < n; i++) {
        int x = -1, y = -1;
        if (!livedesk_read_pos(paths[i], &x, &y)) continue;
        char glyph[64] = "";
        livedesk_glyph(paths[i], glyph, sizeof(glyph));
        char rel[PATH_BUF];
        livedesk_rel_path(house_root, paths[i], rel, sizeof(rel));
        fprintf(w, "DESK | %s | %s | %d | %d | %d | %d | %s | %d\n",
                ents[i], rel, x, y, x / LIVEDESK_GRID_PX, y / LIVEDESK_GRID_PX,
                glyph, indexes[i]);
    }
    fclose(w);
}

/* CLOSE every live entity through its interact_relay.txt, then wait a beat
 * so the entity processes poll the relay and exit before respawns. */
static void livedesk_close_all(const char *house_root) {
    int pids[LIVEDESK_MAX_OPEN], idx[LIVEDESK_MAX_OPEN];
    char ents[LIVEDESK_MAX_OPEN][128], paths[LIVEDESK_MAX_OPEN][PATH_BUF];
    int n = livedesk_read_open(house_root, pids, ents, paths, idx, LIVEDESK_MAX_OPEN);
    for (int i = 0; i < n; i++) {
        char relay[PATH_BUF];
        snprintf(relay, sizeof(relay), "%s/interact_relay.txt", paths[i]);
        FILE *cf = fopen(relay, "w");
        if (cf) { fprintf(cf, "CLOSE\n"); fclose(cf); }
    }
    if (n > 0) {
        struct timespec ts = {0, 450 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
}

/* Spawn every row of <id>/desks/<desk>.pdl. Each row's x,y is written back
 * into the entity package's desktop_pos.txt first so tp_desktop_window.+x
 * (which reads that file at startup) lands exactly on the saved cell. */
static void livedesk_spawn_desk(const char *house_root, const char *sroot, const char *id, const char *desk) {
    char sdir[PATH_BUF], dp[PATH_BUF];
    livedesk_session_dir(sroot, id, sdir, sizeof(sdir));
    snprintf(dp, sizeof(dp), "%s/desks/%s.pdl", sdir, desk);
    FILE *f = fopen(dp, "r");
    if (!f) return;
    char exe[PATH_BUF];
    snprintf(exe, sizeof(exe), "%s/&.widgits/tile-picker/ops/+x/tp_desktop_window.+x", house_root);
    if (access(exe, F_OK) != 0) {
        fclose(f);
        return;
    }
    char line[PATH_BUF * 2];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DESK", 4) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        char *ent = NULL, *path = NULL, *xs = NULL, *ys = NULL;
        p++;
        ent = strtok(p, "|");
        path = strtok(NULL, "|");
        xs = strtok(NULL, "|");
        ys = strtok(NULL, "|");
        (void)ent;
        if (!path) continue;
        while (*path == ' ') path++;
        path[strcspn(path, "\r\n")] = '\0';
        char *pe = path + strlen(path);
        while (pe > path && pe[-1] == ' ') *--pe = '\0';
        char full[PATH_BUF];
        livedesk_join_path(house_root, path, full, sizeof(full));
        if (access(full, F_OK) != 0) continue;
        int x = xs ? atoi(xs) : -1, y = ys ? atoi(ys) : -1;
        if (x >= 0 && y >= 0) {
            char posp[PATH_BUF];
            snprintf(posp, sizeof(posp), "%s/desktop_pos.txt", full);
            FILE *pw = fopen(posp, "w");
            if (pw) { fprintf(pw, "x=%d\ny=%d\n", x, y); fclose(pw); }
        }
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "setsid nohup '%s' '%s' >/dev/null 2>&1 < /dev/null &", exe, full);
        int rc = system(cmd);
        (void)rc;
    }
    fclose(f);
}

/* Short-term default (Q4): on first livedesk action, create session s1
 * "pre-design" snapshotted from the CURRENT live desktop. */
static void livedesk_default_session(const char *house_root, const char *sroot, char *out, size_t sz) {
    char active[PATH_BUF] = "";
    livedesk_root_read(sroot, active, sizeof(active), NULL, 0);
    if (!active[0]) {
        snprintf(out, sz, "s1");
        livedesk_ensure_session(sroot, out, "pre-design");
        livedesk_snapshot_desk(house_root, sroot, out);
        livedesk_root_write(sroot, out, "");
    } else {
        snprintf(out, sz, "%s", active);
    }
}

static void livedesk_switch_desk(const char *house_root, const char *sroot, const char *id, const char *desk) {
    char active[64] = "";
    livedesk_active_desk(sroot, id, active, sizeof(active));
    /* Auto-save the OUTGOING desk only - never the desk we're switching TO
     * (snapshotting the incoming desk from the live registry could wipe its
     * saved entity set when the live desktop is empty/dead). */
    if (strcmp(active, desk) != 0)
        livedesk_snapshot_desk(house_root, sroot, id);
    livedesk_close_all(house_root);
    livedesk_write_active_desk(sroot, id, desk);
    livedesk_spawn_desk(house_root, sroot, id, desk);
}

static void livedesk_load_session(const char *house_root, const char *sroot, const char *id) {
    char cur[PATH_BUF] = "";
    livedesk_root_read(sroot, cur, sizeof(cur), NULL, 0);
    if (cur[0] && strcmp(cur, id) != 0)
        livedesk_snapshot_desk(house_root, sroot, cur);   /* don't lose outgoing work */
    livedesk_close_all(house_root);
    char desk[64] = "";
    livedesk_active_desk(sroot, id, desk, sizeof(desk));
    livedesk_spawn_desk(house_root, sroot, id, desk);
    livedesk_root_write(sroot, id, cur[0] ? cur : id);
}

static void livedesk_new_session(const char *house_root) {
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[PATH_BUF] = "";
    livedesk_root_read(sroot, cur, sizeof(cur), NULL, 0);
    if (cur[0]) livedesk_snapshot_desk(house_root, sroot, cur);
    char id[64];
    int num = livedesk_next_id(sroot, id, sizeof(id));
    char name[64];
    snprintf(name, sizeof(name), "session%d", num);
    livedesk_ensure_session(sroot, id, name);
    livedesk_close_all(house_root);
    livedesk_write_active_desk(sroot, id, "desk_01");
    livedesk_root_write(sroot, id, cur[0] ? cur : id);
}

static void livedesk_new_desk(const char *house_root) {
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    char sdir[PATH_BUF], desks[PATH_BUF], nd[64], dp[PATH_BUF];
    livedesk_session_dir(sroot, cur, sdir, sizeof(sdir));
    snprintf(desks, sizeof(desks), "%s/desks", sdir);
    livedesk_mkdir_p(desks);
    livedesk_next_desk(sroot, cur, nd, sizeof(nd));
    snprintf(dp, sizeof(dp), "%s/%s.pdl", desks, nd);
    FILE *f = fopen(dp, "w");
    if (f) fclose(f);
    livedesk_snapshot_desk(house_root, sroot, cur);   /* auto-save current desk */
    livedesk_close_all(house_root);
    livedesk_write_active_desk(sroot, cur, nd);
    livedesk_root_write(sroot, cur, "");
}

static void livedesk_save(const char *house_root) {
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    livedesk_snapshot_desk(house_root, sroot, cur);
    livedesk_root_write(sroot, cur, "");
}

static void livedesk_save_as(const char *house_root) {
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return;
    char cur[PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    livedesk_snapshot_desk(house_root, sroot, cur);
    char nid[64];
    int num = livedesk_next_id(sroot, nid, sizeof(nid));
    char name[64];
    snprintf(name, sizeof(name), "session%d", num);
    char srcd[PATH_BUF], dst[PATH_BUF];
    livedesk_session_dir(sroot, cur, srcd, sizeof(srcd));
    livedesk_session_dir(sroot, nid, dst, sizeof(dst));
    char cmd[PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s' 2>/dev/null", srcd, dst);
    int rc = system(cmd);
    (void)rc;
    if (access(dst, F_OK) == 0)
        livedesk_set_name(sroot, nid, name);
    else
        livedesk_ensure_session(sroot, nid, name);
    livedesk_close_all(house_root);
    char ad[64] = "";
    livedesk_active_desk(sroot, nid, ad, sizeof(ad));
    livedesk_spawn_desk(house_root, sroot, nid, ad);
    livedesk_root_write(sroot, nid, cur[0] ? cur : nid);
}

static int livedesk_build_session_menu(const char *house_root, HQMenuItem *menu, int max) {
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char ids[LIVEDESK_DYN_MAX][64];
    int n = 0;
    DIR *d = opendir(sroot);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (n >= max || n >= LIVEDESK_DYN_MAX) break;
            if (e->d_name[0] == '.') continue;
            char sp[PATH_BUF];
            snprintf(sp, sizeof(sp), "%s/%s/session.pdl", sroot, e->d_name);
            if (access(sp, F_OK) != 0) continue;
            snprintf(ids[n], sizeof(ids[n]), "%s", e->d_name);
            n++;
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (strcmp(ids[j], ids[i]) < 0) {
                    char t[64];
                    snprintf(t, sizeof(t), "%s", ids[i]);
                    snprintf(ids[i], sizeof(ids[i]), "%s", ids[j]);
                    snprintf(ids[j], sizeof(ids[j]), "%s", t);
                }
    }
    for (int i = 0; i < n && i < max; i++) {
        char name[256] = "";
        livedesk_session_name(sroot, ids[i], name, sizeof(name));
        snprintf(menu[i].label, sizeof(menu[i].label), "%s", name);
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:open-session:%s", ids[i]);
    }
    return n;
}

static int livedesk_build_desk_menu(const char *house_root, HQMenuItem *menu, int max) {
    char sroot[PATH_BUF];
    if (!livedesk_sessions_root(house_root, sroot, sizeof(sroot))) return 0;
    char cur[PATH_BUF] = "";
    livedesk_default_session(house_root, sroot, cur, sizeof(cur));
    char desks[LIVEDESK_DYN_MAX][64];
    int n = livedesk_desk_list(sroot, cur, desks, LIVEDESK_DYN_MAX);
    int i = 0;
    for (; i < n && i < max - 2; i++) {
        snprintf(menu[i].label, sizeof(menu[i].label), "%s", desks[i]);
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:switch-desk:%s/%s", cur, desks[i]);
    }
    if (i < max) {
        snprintf(menu[i].label, sizeof(menu[i].label), "+new-desk");
        snprintf(menu[i].command, sizeof(menu[i].command), "livedesk:new-desk");
        i++;
    }
    if (i < max) {
        snprintf(menu[i].label, sizeof(menu[i].label), "cancel");
        menu[i].command[0] = '\0';
        i++;
    }
    return i;
}

/* Reuse the strip-popup machinery for a dynamic (disk-built) row list. */
static void livedesk_open_dyn_popup(Display *dpy, GC gc, const char *house_root,
                                    HQMenuItem *menu, int n) {
    if (n <= 0) return;
    int px = g_livedesk_popup_x ? g_livedesk_popup_x : g_strip_x_offset;
    int py = g_strip_y_offset + BAR_H;
    Window cur = g_strip_popup_open ? g_strip_popup : (g_hq_popup_open ? g_hq_popup : 0);
    if (cur) {
        Window r;
        int rx = 0, ry = 0;
        unsigned uw = 0, uh = 0, b = 0, d = 0;
        if (XGetGeometry(dpy, cur, &r, &rx, &ry, &uw, &uh, &b, &d)) { px = rx; py = ry; }
    }
    close_popups(dpy, house_root);
    popup_claim(house_root, (int)getpid(), menu, n);
    g_strip_popup = open_hq_popup(dpy, gc, house_root, px, py, menu, n);
    g_strip_popup_open = 1;
    g_strip_popup_focus = 0;
    g_strip_popup_digit_accum = 0;
    g_strip_popup_n = n;
    g_strip_popup_menu = menu;
    taskbar_soft_focus(dpy, g_strip_popup);
}

static void livedesk_open_sessions_popup(Display *dpy, GC gc, const char *house_root) {
    int n = livedesk_build_session_menu(house_root, g_livedesk_dyn, LIVEDESK_DYN_MAX);
    if (n <= 0) {
        snprintf(g_livedesk_dyn[0].label, sizeof(g_livedesk_dyn[0].label), "(no sessions - File->new)");
        g_livedesk_dyn[0].command[0] = '\0';
        n = 1;
    }
    livedesk_open_dyn_popup(dpy, gc, house_root, g_livedesk_dyn, n);
}

static void livedesk_open_desks_popup(Display *dpy, GC gc, const char *house_root) {
    int n = livedesk_build_desk_menu(house_root, g_livedesk_dyn, LIVEDESK_DYN_MAX);
    if (n <= 0) {
        snprintf(g_livedesk_dyn[0].label, sizeof(g_livedesk_dyn[0].label), "(no desks)");
        g_livedesk_dyn[0].command[0] = '\0';
        n = 1;
    }
    livedesk_open_dyn_popup(dpy, gc, house_root, g_livedesk_dyn, n);
}

static void livedesk_dispatch(Display *dpy, GC gc, const char *house_root, const char *cmd) {
    if (strncmp(cmd, "livedesk:", 9) != 0) return;
    const char *c = cmd + 9;
    if (strcmp(c, "save") == 0) livedesk_save(house_root);
    else if (strcmp(c, "save-as") == 0) livedesk_save_as(house_root);
    else if (strcmp(c, "new") == 0) livedesk_new_session(house_root);
    else if (strcmp(c, "new-desk") == 0) livedesk_new_desk(house_root);
    else if (strcmp(c, "load") == 0) livedesk_open_sessions_popup(dpy, gc, house_root);
    else if (strcmp(c, "desks") == 0) livedesk_open_desks_popup(dpy, gc, house_root);
    else if (strncmp(c, "open-session:", 13) == 0) {
        char sroot[PATH_BUF];
        if (livedesk_sessions_root(house_root, sroot, sizeof(sroot)))
            livedesk_load_session(house_root, sroot, c + 13);
    } else if (strncmp(c, "switch-desk:", 12) == 0) {
        const char *sub = c + 12;
        const char *slash = strrchr(sub, '/');
        char sroot[PATH_BUF];
        if (slash && livedesk_sessions_root(house_root, sroot, sizeof(sroot))) {
            char id[128];
            size_t len = (size_t)(slash - sub);
            if (len >= sizeof(id)) len = sizeof(id) - 1;
            memcpy(id, sub, len);
            id[len] = '\0';
            livedesk_switch_desk(house_root, sroot, id, slash + 1);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_taskbar.+x <house_root>\n");
        return 1;
    }
    const char *house_root = argv[1];
    snprintf(g_house_root, sizeof(g_house_root), "%s", house_root);

    /* Anchor the process to house_root so relative commands in the HQ menu
     * pdl (e.g. "setsid nohup $.crypts/button.sh run") resolve against the
     * house root regardless of where this binary was launched from. Every
     * file path this program touches is house_root-joined (see join paths
     * below), so chdir is safe. */
    if (chdir(house_root) != 0) {
        fprintf(stderr, "tp_taskbar: cannot chdir(%s)\n", house_root);
        return 1;
    }

    char pid_path[PATH_BUF];
    snprintf(pid_path, sizeof(pid_path), "%s/#.desktop/livedesk_taskbar.pid", house_root);
    FILE *pf = fopen(pid_path, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    /* Initialize debug directory and frame log files so they're never null when zipping */
    char dbg_dir[PATH_BUF];
    snprintf(dbg_dir, sizeof(dbg_dir), "%s/#.desktop/tp_taskbar_debug", house_root);
    mkdir(dbg_dir, 0755);
    char init_files[4][PATH_BUF];
    snprintf(init_files[0], sizeof(init_files[0]), "%s/strip_frame_log.txt", dbg_dir);
    snprintf(init_files[1], sizeof(init_files[1]), "%s/popup_frame_log.txt", dbg_dir);
    snprintf(init_files[2], sizeof(init_files[2]), "%s/key_history.txt", dbg_dir);
    snprintf(init_files[3], sizeof(init_files[3]), "%s/strip_frame_changed.txt", dbg_dir);
    for (int i = 0; i < 4; i++) {
        FILE *f = fopen(init_files[i], "a");
        if (f) fclose(f);
    }

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
    double theme_opacity = load_theme_opacity(house_root); /* #.desktop/livedesk_theme.pdl: COLOR | opacity | 0.5 */

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg_pixel;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window win = create_bar_window(dpy, &swa, 0, screen_h - BAR_H, screen_w, BAR_H, theme_opacity);
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
        snprintf(userpal_root, sizeof(userpal_root), "%s/0.user-pal👤️/00.login-signup", house_root);
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

    char hq_label[16] = "HQ";
    HQMenuItem hq_menu[HQ_MENU_MAX];
    int hq_n_menu = 0;
    load_hq_config(house_root, hq_label, sizeof(hq_label), hq_menu, HQ_MENU_MAX, &hq_n_menu);

    /* REAL, 2026-08-08, direct instruction ("top-left command strip"): a
     * persistent second strip pinned at the top-left of the screen in this
     * same process (opens when the taskbar opens, stays open). Order:
     * HQ (farthest left, leaves its old right-edge spot), user/guest tag
     * (moved in from top-middle-right, DIRECT INSTRUCTION: before "file"),
     * file, desks, player, db, plugins. */
    StripBtn strip_btns[STRIP_BTN_MAX];
    int strip_n_btns = 0;
    int strip_x_offset = 0;
    int strip_y_offset = 40;
    char strip_user_cmd[PATH_BUF] = "";
    load_strip_config(house_root, strip_btns, STRIP_BTN_MAX, &strip_n_btns,
                      &strip_x_offset, &strip_y_offset, strip_user_cmd, sizeof(strip_user_cmd));

    StripCell cells[STRIP_BTN_MAX + 2];
    int n_cells = 0;
    snprintf(cells[n_cells].label, sizeof(cells[n_cells].label), "%s", hq_label);
    cells[n_cells].menu = hq_menu;
    cells[n_cells].n_menu = hq_n_menu;
    cells[n_cells].cmd = NULL;
    cells[n_cells].is_static = 0;
    n_cells++;
    /* REAL, 2026-08-08, direct instruction ("user will also be a button,
     * can be used to change users later"): the user/guest tag is a real
     * button too - its command comes from strip_user_cmd (empty for now,
     * wired to a user-switcher later), so clicking it is a no-op today
     * instead of being a dead text label. */
    snprintf(cells[n_cells].label, sizeof(cells[n_cells].label), "%s", user_label);
    cells[n_cells].menu = NULL;
    cells[n_cells].n_menu = 0;
    cells[n_cells].cmd = strip_user_cmd;
    cells[n_cells].is_static = 0;
    active_avatar_dir(house_root, cells[n_cells].sprite_path, sizeof(cells[n_cells].sprite_path));
    n_cells++;
    for (int i = 0; i < strip_n_btns && n_cells < STRIP_BTN_MAX + 2; i++) {
        snprintf(cells[n_cells].label, sizeof(cells[n_cells].label), "%s", strip_btns[i].label);
        cells[n_cells].menu = strip_btns[i].menu;
        cells[n_cells].n_menu = strip_btns[i].n_menu;
        cells[n_cells].cmd = strip_btns[i].command;
        cells[n_cells].is_static = 0;
        n_cells++;
    }
    /* REAL, 2026-08-08, DIRECT INSTRUCTION ("hq should have a nav number,
     * and nav numbers grow"): strip buttons claim real nav numbers from the
     * shared pool BEFORE the window is sized, so the strip is wide enough to
     * show them from the first frame. */
    sync_strip_claims(house_root, cells, n_cells);
    /* file button doubles as the current <project/session> name display:
     * "file:<name>" (cells[2] is always the first strip button - HQ and
     * USER cells come first). Sized before the strip width is computed so
     * the label fits from frame one; refreshed on each poll tick. */
    {
        char sname[128] = "", flab[96];
        if (livedesk_current_session_name(house_root, sname, sizeof(sname)) && sname[0])
            snprintf(flab, sizeof(flab), "file:%s", sname);
        else
            snprintf(flab, sizeof(flab), "file");
        snprintf(g_file_label, sizeof(g_file_label), "%s", flab);
        if (n_cells > 2) snprintf(cells[2].label, sizeof(cells[2].label), "%s", flab);
    }
    int strip_w = 8 + STRIP_NAV_BOX_W; /* reserved left space for the [NAV]/[<digits>] indicator, moved here from the bottom bar */
    for (int i = 0; i < n_cells; i++) {
        char dstr[160];
        format_strip_cell(&cells[i], 0, 0, dstr, sizeof(dstr));
        int w = (int)strlen(dstr) * 8 + 20;
        if (w < 40) w = 40;
        strip_w += w + STRIP_PAD;
    }
    /* REAL, 2026-08-09, DIRECT INSTRUCTION ("i like that position...leave
     * it there from now on"): strip_y_offset defaults to 40 (below GNOME
     * Shell's own native top panel) - originally an opacity diagnostic
     * (opacity-bug-aug9.txt Theory 5, since ruled out as the actual
     * opacity cause), kept permanently as the real default position
     * because it's simply preferred. PDL-overridable via
     * #.desktop/livedesk_taskbar.pdl's own strip_y_offset key, same as
     * strip_x_offset. */
    Window strip_win = create_bar_window(dpy, &swa, strip_x_offset, strip_y_offset, strip_w, BAR_H, theme_opacity);
    g_strip_x_offset = strip_x_offset;
    g_strip_y_offset = strip_y_offset;
    g_focus_restore_win = win;
    draw_strip(dpy, strip_win, gc, strip_w, cells, n_cells, bg_pixel, 0, NULL);
    /* Capture initial frame immediately so strip_frame_log.txt is never empty/null */
    mark_strip_frame_changed(house_root, "startup");
    capture_strip_frame(dpy, strip_win, house_root, strip_w, BAR_H, cells, n_cells, 0);
    g_last_strip_marker_size = 1; /* force next poll to check for real changes */

    Tab tabs[MAX_TABS];
    int n_tabs = 0;
    int nav_armed = 0;       /* right-click arms arrows + # digit jump (wraith-style) */
    int tab_focus_idx = 0;
    int nav_focus = 0;       /* SINGLE unified cursor over [strip buttons][tabs] - defaults to button 1, chtpm-style, see nav_focus_apply() */
    char digit_buf[16] = "";  /* typed index while armed — no middle Nav box */

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
            sync_strip_claims(house_root, cells, n_cells);
            if (n_tabs <= 0) tab_focus_idx = 0;
            else if (tab_focus_idx >= n_tabs) tab_focus_idx = n_tabs - 1;
            char sname[128] = "", flab[96];
            if (livedesk_current_session_name(house_root, sname, sizeof(sname)) && sname[0])
                snprintf(flab, sizeof(flab), "file:%s", sname);
            else
                snprintf(flab, sizeof(flab), "file");
            if (strcmp(flab, g_file_label) != 0) {
                snprintf(g_file_label, sizeof(g_file_label), "%s", flab);
                if (n_cells > 2) {
                    snprintf(cells[2].label, sizeof(cells[2].label), "%s", flab);
                    mark_strip_frame_changed(house_root, "session-label");
                }
            }
            need_redraw = 1;
            mark_strip_frame_changed(house_root, "poll-sync");
            draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
            last_poll = now;
        }

        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (xev.type == Expose && xev.xany.window == strip_win) {
                draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
            } else if (xev.type == Expose && g_hq_popup_open && xev.xany.window == g_hq_popup) {
                draw_hq_popup(dpy, g_hq_popup, gc, hq_menu, hq_n_menu, g_hq_focus_row);
            } else if (xev.type == Expose && g_strip_popup_open && xev.xany.window == g_strip_popup) {
                draw_hq_popup(dpy, g_strip_popup, gc, g_strip_popup_menu, g_strip_popup_n, g_strip_popup_focus);
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xany.window == strip_win) {
                int btn = xev.xbutton.button;
                if (btn == 3) {
                    close_popups(dpy, house_root);
                    nav_armed = 1;
                    /* Arm always starts the SINGLE unified cursor at index 0 -
                     * the first strip button (the header) - per direct
                     * instruction that the header gets priority [>] focus. */
                    nav_focus = 0;
                    nav_focus_apply(nav_focus, n_cells, &strip_focus_cell, &tab_focus_idx);
                    digit_buf[0] = '\0';
                    mark_strip_frame_changed(house_root, "right-click-arm");
                    /* REAL BUG FIX, 2026-08-09 (direct instruction: "still
                     * starting focus in 8 instead of 1"): draw_strip_if_marked()
                     * was previously only reached from the once-a-second poll
                     * tick or an Expose event - arming here only grew the
                     * marker file and set need_redraw (which only drives
                     * draw_bar, the BOTTOM bar). The strip window itself kept
                     * showing whatever it last drew, up to ~1s stale, while
                     * draw_bar redrew instantly every keypress - looked
                     * exactly like "focus stuck on the bottom bar" even after
                     * the two-cursor bug was fixed. Every state-changing event
                     * must redraw the strip SYNCHRONOUSLY, matching "we only
                     * render from frame change" - the frame DID just change,
                     * so it renders NOW, not on the next poll. */
                    draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
                    taskbar_soft_focus(dpy, win);
                } else {
                    int bx = (int)xev.xbutton.x;
                    for (int i = 0; i < n_cells; i++) {
                        if (bx < cells[i].x0 || bx >= cells[i].x1) continue;
                        if (cells[i].is_static) break;
                        open_cell_popup(dpy, gc, house_root, cells, n_cells, i, hq_menu, hq_n_menu);
                        break;
                    }
                }
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xany.window == win) {
                int btn = xev.xbutton.button;
                if (btn == 3) {
                    close_popups(dpy, house_root);
                    nav_armed = 1;
                    /* Arm always starts the SINGLE unified cursor at index 0 -
                     * the first strip button (the header) - per direct
                     * instruction that the header gets priority [>] focus. */
                    nav_focus = 0;
                    nav_focus_apply(nav_focus, n_cells, &strip_focus_cell, &tab_focus_idx);
                    digit_buf[0] = '\0';
                    mark_strip_frame_changed(house_root, "right-click-arm");
                    /* REAL BUG FIX, 2026-08-09 (direct instruction: "still
                     * starting focus in 8 instead of 1"): draw_strip_if_marked()
                     * was previously only reached from the once-a-second poll
                     * tick or an Expose event - arming here only grew the
                     * marker file and set need_redraw (which only drives
                     * draw_bar, the BOTTOM bar). The strip window itself kept
                     * showing whatever it last drew, up to ~1s stale, while
                     * draw_bar redrew instantly every keypress - looked
                     * exactly like "focus stuck on the bottom bar" even after
                     * the two-cursor bug was fixed. Every state-changing event
                     * must redraw the strip SYNCHRONOUSLY, matching "we only
                     * render from frame change" - the frame DID just change,
                     * so it renders NOW, not on the next poll. */
                    draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
                    taskbar_soft_focus(dpy, win);
                } else if (btn == 1) {
                    close_popups(dpy, house_root);
                    nav_armed = 0;
                    digit_buf[0] = '\0';
                    int idx = xev.xbutton.x / TAB_W;
                    if (idx >= 0 && idx < n_tabs && (idx + 1) * TAB_W <= screen_w - 8) {
                        tab_focus_idx = idx;
                        taskbar_raise_tab(dpy, tabs, n_tabs, idx);
                    }
                }
                need_redraw = 1;
            } else if (xev.type == ButtonPress && g_hq_popup_open && xev.xany.window == g_hq_popup) {
                int row_y = (int)xev.xbutton.y;
                int clicked_row = row_y / HQ_POPUP_ROW_H;
                if (clicked_row >= 0 && clicked_row < hq_n_menu) {
                    run_popup_row(dpy, gc, house_root, tabs, &n_tabs, pid_path, hq_menu, clicked_row, &running);
                }
                close_popups(dpy, house_root);
                need_redraw = 1;
            } else if (xev.type == ButtonPress && g_strip_popup_open && xev.xany.window == g_strip_popup) {
                int row_y = (int)xev.xbutton.y;
                int clicked_row = row_y / HQ_POPUP_ROW_H;
                if (clicked_row >= 0 && clicked_row < g_strip_popup_n) {
                    run_popup_row(dpy, gc, house_root, tabs, &n_tabs, pid_path, g_strip_popup_menu, clicked_row, &running);
                }
                close_popups(dpy, house_root);
                need_redraw = 1;
            } else if (xev.type == KeyPress && remote_entity_menu_open(house_root)) {
                /* A remote entity (different PID) has a context menu open.
                 * Yield keyboard focus to that entity - don't consume the event. */
                /* Simply ignore KeyPress; entity's own process will handle it. */
            } else if (xev.type == KeyPress && g_hq_popup_open) {
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Escape) {
                    close_popups(dpy, house_root);
                    need_redraw = 1;
                } else if (ks == XK_Up) {
                    if (hq_n_menu > 0) {
                        g_hq_focus_row = (g_hq_focus_row - 1 + hq_n_menu) % hq_n_menu;
                        g_hq_digit_accum = 0;
                        draw_hq_popup(dpy, g_hq_popup, gc, hq_menu, hq_n_menu, g_hq_focus_row);
                    }
                } else if (ks == XK_Down) {
                    if (hq_n_menu > 0) {
                        g_hq_focus_row = (g_hq_focus_row + 1) % hq_n_menu;
                        g_hq_digit_accum = 0;
                        draw_hq_popup(dpy, g_hq_popup, gc, hq_menu, hq_n_menu, g_hq_focus_row);
                    }
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (g_hq_focus_row >= 0 && g_hq_focus_row < hq_n_menu) {
                        run_popup_row(dpy, gc, house_root, tabs, &n_tabs, pid_path, hq_menu, g_hq_focus_row, &running);
                    }
                    close_popups(dpy, house_root);
                    need_redraw = 1;
                } else if (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                    if (popup_digit(hq_menu, hq_n_menu, &g_hq_digit_accum, &g_hq_focus_row, kbuf[0] - '0'))
                        draw_hq_popup(dpy, g_hq_popup, gc, hq_menu, hq_n_menu, g_hq_focus_row);
                }
            } else if (xev.type == KeyPress && g_strip_popup_open) {
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Escape) {
                    close_popups(dpy, house_root);
                    need_redraw = 1;
                } else if (ks == XK_Up) {
                    if (g_strip_popup_n > 0) {
                        g_strip_popup_focus = (g_strip_popup_focus - 1 + g_strip_popup_n) % g_strip_popup_n;
                        g_strip_popup_digit_accum = 0;
                        draw_hq_popup(dpy, g_strip_popup, gc, g_strip_popup_menu, g_strip_popup_n, g_strip_popup_focus);
                    }
                } else if (ks == XK_Down) {
                    if (g_strip_popup_n > 0) {
                        g_strip_popup_focus = (g_strip_popup_focus + 1) % g_strip_popup_n;
                        g_strip_popup_digit_accum = 0;
                        draw_hq_popup(dpy, g_strip_popup, gc, g_strip_popup_menu, g_strip_popup_n, g_strip_popup_focus);
                    }
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (g_strip_popup_focus >= 0 && g_strip_popup_focus < g_strip_popup_n) {
                        run_popup_row(dpy, gc, house_root, tabs, &n_tabs, pid_path, g_strip_popup_menu, g_strip_popup_focus, &running);
                    }
                    close_popups(dpy, house_root);
                    need_redraw = 1;
                } else if (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                    if (popup_digit(g_strip_popup_menu, g_strip_popup_n, &g_strip_popup_digit_accum,
                                    &g_strip_popup_focus, kbuf[0] - '0'))
                        draw_hq_popup(dpy, g_strip_popup, gc, g_strip_popup_menu, g_strip_popup_n, g_strip_popup_focus);
                }
            } else if (xev.type == KeyPress && nav_armed) {
                char kbuf[8];
                KeySym ks;
                int klen = XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                /* History gets the raw key FIRST, before any state changes below
                 * (keyboard_input.c's append_key() ordering). */
                {
                    const char *ks_name = (ks == XK_Left) ? "Left" : (ks == XK_Right) ? "Right" :
                                          (ks == XK_Up) ? "Up" : (ks == XK_Down) ? "Down" :
                                          (ks == XK_Return || ks == XK_KP_Enter) ? "Return" :
                                          (ks == XK_Escape) ? "Escape" : (ks == XK_BackSpace) ? "BackSpace" :
                                          (klen > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') ? "Digit" : "Other";
                    append_key_history(house_root, ks_name, strip_focus_cell, tab_focus_idx);
                }
                /* Sync the unified cursor from wherever it currently is
                 * (e.g. a prior digit-jump landed on a specific tab/button)
                 * before stepping it - see nav_focus_apply()'s own header
                 * comment for why this ONE variable is now the only thing
                 * arrow keys ever move. */
                if (nav_focus < 0) {
                    nav_focus = (strip_focus_cell >= 0) ? strip_focus_cell : n_cells + tab_focus_idx;
                }
                if (ks == XK_Left || ks == XK_Up) {
                    nav_focus_step(cells, n_cells, n_tabs, &nav_focus, -1);
                    nav_focus_apply(nav_focus, n_cells, &strip_focus_cell, &tab_focus_idx);
                    digit_buf[0] = '\0'; /* chtpm: arrows reset digit_accum */
                    mark_strip_frame_changed(house_root, "nav-left");
                    draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
                } else if (ks == XK_Right || ks == XK_Down) {
                    nav_focus_step(cells, n_cells, n_tabs, &nav_focus, 1);
                    nav_focus_apply(nav_focus, n_cells, &strip_focus_cell, &tab_focus_idx);
                    digit_buf[0] = '\0';
                    mark_strip_frame_changed(house_root, "nav-right");
                    draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (digit_buf[0] == '\0') {
                        /* Activate focused button or tab */
                        if (strip_focus_cell >= 0) {
                            /* Focused on a strip button */
                            open_cell_popup(dpy, gc, house_root, cells, n_cells, strip_focus_cell, hq_menu, hq_n_menu);
                        } else if (n_tabs > 0) {
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
                                            strip_focus_cell = -1;
                                            nav_focus = n_cells + ti; /* keep unified cursor in sync so the next arrow key continues from here */
                                            taskbar_activate_tab(dpy, tabs, n_tabs, ti);
                                            break;
                                        }
                                    }
                                } else if (strcmp(kind, "btn") == 0) {
                                    int ci = cell_for_nav(cells, n_cells, nav_n);
                                    if (ci >= 0) {
                                        strip_focus_cell = ci;
                                        nav_focus = ci;
                                        open_cell_popup(dpy, gc, house_root, cells, n_cells, ci, hq_menu, hq_n_menu);
                                    }
                                } else {
                                    /* Our own open popup row (PATH is the
                                     * house root, no entity relay): move [>]
                                     * to it right here instead. */
                                    if (strcmp(path, house_root) == 0) {
                                        if (g_hq_popup_open) {
                                            for (int r = 0; r < hq_n_menu; r++) {
                                                if (hq_menu[r].nav == nav_n) {
                                                    g_hq_focus_row = r;
                                                    draw_hq_popup(dpy, g_hq_popup, gc, hq_menu, hq_n_menu, g_hq_focus_row);
                                                    break;
                                                }
                                            }
                                        } else if (g_strip_popup_open) {
                                            for (int r = 0; r < g_strip_popup_n; r++) {
                                                if (g_strip_popup_menu[r].nav == nav_n) {
                                                    g_strip_popup_focus = r;
                                                    draw_hq_popup(dpy, g_strip_popup, gc, g_strip_popup_menu, g_strip_popup_n, g_strip_popup_focus);
                                                    break;
                                                }
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
                                        strip_focus_cell = -1;
                                        nav_focus = n_cells + ti;
                                        break;
                                    }
                                }
                            } else if (strcmp(kind, "btn") == 0) {
                                int ci = cell_for_nav(cells, n_cells, nav_n);
                                if (ci >= 0) {
                                    strip_focus_cell = ci;
                                    nav_focus = ci;
                                    /* Do NOT open popup here - defer activation to Enter key */
                                    mark_strip_frame_changed(house_root, "digit-btn-focus");
                                    draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
                                }
                            } else if (strcmp(kind, "row") == 0 && path[0]) {
                                if (strcmp(path, house_root) == 0) {
                                    if (g_hq_popup_open) {
                                        for (int r = 0; r < hq_n_menu; r++) {
                                            if (hq_menu[r].nav == nav_n) {
                                                g_hq_focus_row = r;
                                                draw_hq_popup(dpy, g_hq_popup, gc, hq_menu, hq_n_menu, g_hq_focus_row);
                                                break;
                                            }
                                        }
                                    } else if (g_strip_popup_open) {
                                        for (int r = 0; r < g_strip_popup_n; r++) {
                                            if (g_strip_popup_menu[r].nav == nav_n) {
                                                g_strip_popup_focus = r;
                                                draw_hq_popup(dpy, g_strip_popup, gc, g_strip_popup_menu, g_strip_popup_n, g_strip_popup_focus);
                                                break;
                                            }
                                        }
                                    }
                                } else {
                                    char relay[PATH_BUF];
                                    snprintf(relay, sizeof(relay), "%s/interact_relay.txt", path);
                                    FILE *rf = fopen(relay, "w");
                                    if (rf) { fprintf(rf, "FOCUS_NAV:%d\n", nav_n); fclose(rf); }
                                }
                            }
                        }
                    }
                }
                need_redraw = 1;
            }
        }

        if (need_redraw) {
            draw_bar(dpy, win, gc, screen_w, tabs, n_tabs, nav_armed, (int)strlen(digit_buf), hq_label, bg_pixel, tab_focus_idx, digit_buf, strip_focus_cell);
            /* Catch-all so the strip's own [NAV]/[<digits>] box (now living
             * there, not the bottom bar) stays live for every state change
             * that sets need_redraw but doesn't already call
             * mark_strip_frame_changed() itself - e.g. digit typing,
             * backspace, escape. */
            mark_strip_frame_changed(house_root, "need-redraw");
            draw_strip_if_marked(dpy, strip_win, gc, house_root, strip_w, BAR_H, cells, n_cells, bg_pixel, nav_armed, digit_buf);
        }
    }

    XCloseDisplay(dpy);
    unlink(pid_path);
    return 0;
}
