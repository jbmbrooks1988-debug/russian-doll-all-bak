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

#define HQ_BTN_W 40
#define HQ_POPUP_ROW_H 28
#define HQ_MENU_MAX 8
typedef struct {
    char label[64];
    char command[PATH_BUF];
} HQMenuItem;

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

/* REAL, 2026-08-08, direct instruction ("put position in config/.pdl so i
 * can change it on my own — runtime-configurable values are always preferred
 * over hardcoded constants"): reads #.desktop/livedesk_taskbar.pdl for
 * user-tag placement. Missing keys fall back to safe defaults. */
static void load_taskbar_config(const char *house_root, int *user_tag_x_offset, int *user_tag_y) {
    *user_tag_x_offset = 245;
    *user_tag_y = 0;
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
        if (v[0] == '\0') continue;

        if (strcmp(key, "user_tag_x_offset") == 0) *user_tag_x_offset = atoi(v);
        else if (strcmp(key, "user_tag_y") == 0) *user_tag_y = atoi(v);
    }
    fclose(f);
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
    int count = 0;
    for (int i = 0; i < max_menu; i++) {
        if (menu[i].label[0]) count++;
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

    int hq_x0 = screen_w - HQ_BTN_W;
    int tabs_right = hq_x0 - 4;
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
    /* Small status when right-click nav is armed (no typing box). */
    if (nav_armed) {
        const char *arm = digit_buf_len > 0 ? digit_buf : "NAV";
        char arm_lab[64];
        snprintf(arm_lab, sizeof(arm_lab), "[%s]", arm);
        XDrawString(dpy, win, gc, tabs_right - 80, BAR_H / 2 + 4, arm_lab, (int)strlen(arm_lab));
    }

    /* REAL, 2026-08-08, direct instruction ("replace '$' + 'x' with single
     * 'HQ' button that pops up $.restart / X.quit"): single HQ button at
     * the right edge of the bar, replacing the old hardcoded X close button
     * and the generic shortcuts row. */
    XDrawRectangle(dpy, win, gc, hq_x0, 2, HQ_BTN_W - 4, BAR_H - 5);
    XDrawString(dpy, win, gc, hq_x0 + 8, BAR_H / 2 + 4, hq_label, (int)strlen(hq_label));

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
        snprintf(labeled, sizeof(labeled), "%s %s", cursor, menu[i].label);
        XDrawString(dpy, popup, gc, 12, row_y + HQ_POPUP_ROW_H / 2 + 4, labeled, (int)strlen(labeled));
    }
    XSetForeground(dpy, gc, saved.foreground);
}

static void close_hq_popup(Display *dpy, Window popup) {
    if (popup) XDestroyWindow(dpy, popup);
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
    /* REAL FIX 2026-08-08, direct instruction ("move user: tag to top
     * middle-right of screen, between data and battery/volume indicators"):
     * moved from just above the bottom X button to the top of the screen,
     * inset from the right edge to sit inside the system-tray area.
     * Position is now read from livedesk_taskbar.pdl so it can be changed
     * without recompiling. */
    int user_tag_x_offset = 245;
    int user_tag_y = 0;
    load_taskbar_config(house_root, &user_tag_x_offset, &user_tag_y);
    int user_x0 = screen_w - user_w - user_tag_x_offset;
    Window user_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                     user_x0, user_tag_y, user_w, BAR_H, 0,
                                     CopyFromParent, InputOutput, CopyFromParent,
                                     CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    taskbar_set_wm_class(dpy, user_win);
    XMapRaised(dpy, user_win);

    char hq_label[16] = "HQ";
    HQMenuItem hq_menu[HQ_MENU_MAX];
    int hq_n_menu = 0;
    load_hq_config(house_root, hq_label, sizeof(hq_label), hq_menu, HQ_MENU_MAX, &hq_n_menu);

    Tab tabs[MAX_TABS];
    int n_tabs = 0;
    int nav_armed = 0;       /* right-click arms arrows + # digit jump (wraith-style) */
    int tab_focus_idx = 0;
    char digit_buf[16] = "";  /* typed index while armed — no middle Nav box */

    Window hq_popup = 0;
    int hq_popup_open = 0;
    int hq_focus_row = -1;
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
            } else if (xev.type == Expose && hq_popup_open && xev.xany.window == hq_popup) {
                draw_hq_popup(dpy, hq_popup, gc, hq_menu, hq_n_menu, hq_focus_row);
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xany.window == win) {
                int hq_x0 = screen_w - HQ_BTN_W;
                int btn = xev.xbutton.button;

                if (btn == 1 && xev.xbutton.x >= hq_x0) {
                    if (hq_popup_open) {
                        close_hq_popup(dpy, hq_popup);
                        hq_popup = 0;
                        hq_popup_open = 0;
                        hq_focus_row = -1;
                    } else if (hq_n_menu > 0) {
                        int popup_x = hq_x0;
                        int popup_y = screen_h - BAR_H - HQ_POPUP_ROW_H * hq_n_menu;
                        hq_popup = open_hq_popup(dpy, gc, house_root, popup_x, popup_y, hq_menu, hq_n_menu);
                        hq_popup_open = 1;
                        hq_focus_row = 0;
                    }
                } else if (btn == 3) {
                    if (hq_popup_open) {
                        close_hq_popup(dpy, hq_popup);
                        hq_popup = 0;
                        hq_popup_open = 0;
                        hq_focus_row = -1;
                    }
                    nav_armed = 1;
                    digit_buf[0] = '\0';
                    taskbar_soft_focus(dpy, win);
                } else if (btn == 1) {
                    if (hq_popup_open) {
                        close_hq_popup(dpy, hq_popup);
                        hq_popup = 0;
                        hq_popup_open = 0;
                        hq_focus_row = -1;
                    }
                    nav_armed = 0;
                    digit_buf[0] = '\0';
                    int idx = xev.xbutton.x / TAB_W;
                    if (idx >= 0 && idx < n_tabs && (idx + 1) * TAB_W <= hq_x0) {
                        tab_focus_idx = idx;
                        taskbar_raise_tab(dpy, tabs, n_tabs, idx);
                    }
                }
                need_redraw = 1;
            } else if (xev.type == ButtonPress && hq_popup_open && xev.xany.window == hq_popup) {
                int row_y = (int)xev.xbutton.y;
                int clicked_row = row_y / HQ_POPUP_ROW_H;
                if (clicked_row >= 0 && clicked_row < hq_n_menu) {
                    if (strcmp(hq_menu[clicked_row].command, "quit") == 0) {
                        n_tabs = load_tabs(house_root, tabs, MAX_TABS);
                        quit_and_save_session(dpy, house_root, tabs, n_tabs, pid_path);
                        running = 0;
                    } else if (hq_menu[clicked_row].command[0]) {
                        char cmd[PATH_BUF * 2];
                        snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", hq_menu[clicked_row].command);
                        int rc = system(cmd);
                        (void)rc;
                    }
                }
                close_hq_popup(dpy, hq_popup);
                hq_popup = 0;
                hq_popup_open = 0;
                hq_focus_row = -1;
                need_redraw = 1;
            } else if (xev.type == KeyPress && hq_popup_open) {
                char kbuf[8];
                KeySym ks;
                XLookupString(&xev.xkey, kbuf, sizeof(kbuf) - 1, &ks, NULL);
                if (ks == XK_Escape) {
                    close_hq_popup(dpy, hq_popup);
                    hq_popup = 0;
                    hq_popup_open = 0;
                    hq_focus_row = -1;
                    need_redraw = 1;
                } else if (ks == XK_Up && hq_focus_row > 0) {
                    hq_focus_row--;
                    draw_hq_popup(dpy, hq_popup, gc, hq_menu, hq_n_menu, hq_focus_row);
                } else if (ks == XK_Down && hq_focus_row < hq_n_menu - 1) {
                    hq_focus_row++;
                    draw_hq_popup(dpy, hq_popup, gc, hq_menu, hq_n_menu, hq_focus_row);
                } else if (ks == XK_Return || ks == XK_KP_Enter) {
                    if (hq_focus_row >= 0 && hq_focus_row < hq_n_menu) {
                        if (strcmp(hq_menu[hq_focus_row].command, "quit") == 0) {
                            n_tabs = load_tabs(house_root, tabs, MAX_TABS);
                            quit_and_save_session(dpy, house_root, tabs, n_tabs, pid_path);
                            running = 0;
                        } else if (hq_menu[hq_focus_row].command[0]) {
                            char cmd[PATH_BUF * 2];
                            snprintf(cmd, sizeof(cmd), "setsid nohup %s >/dev/null 2>&1 &", hq_menu[hq_focus_row].command);
                            int rc = system(cmd);
                            (void)rc;
                        }
                    }
                    close_hq_popup(dpy, hq_popup);
                    hq_popup = 0;
                    hq_popup_open = 0;
                    hq_focus_row = -1;
                    need_redraw = 1;
                }
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

        if (need_redraw) draw_bar(dpy, win, gc, screen_w, tabs, n_tabs, nav_armed, (int)strlen(digit_buf), hq_label, bg_pixel, tab_focus_idx, digit_buf);
    }

    XCloseDisplay(dpy);
    unlink(pid_path);
    return 0;
}
