/* khtpm_strip_parser.c — the new outer process, playing chtpm_parser.c's
 * role for the taskbar strip (see khtpm-strip-parser-design.md and
 * khtpm-strip-parser-SCOPE.md, the locked implementation scope this pass
 * follows).
 *
 * REAL DECLARATIVE-LAYOUT PASS (2026-08-11): this file's render walk and
 * click/key dispatch walk now run against khtpm_strip_layout.c's tag tree
 * (loaded from ../khtpm_strip_header.chtpm / ../khtpm_strip_bottom.chtpm),
 * NOT the old hardcoded HQ_HEADER_LABELS[]/draw_header_win()/
 * draw_popup_win() per-cell drawing + manual g_header_x0[]/g_header_x1[]
 * hit-testing. Per SCOPE.md: "most of its drawing code ... becomes the
 * OUTPUT of a layout-driven render walk, not hand-written per-cell drawing
 * code." What survives UNCHANGED from the prior pass, per SCOPE.md's own
 * "what survives" list: window creation, opacity (_NET_WM_WINDOW_OPACITY),
 * the offscreen-pixmap-then-XCopyArea drawing pattern (flicker/opacity
 * fix, proven working — kept exactly), the manager fork/exec + lazy-
 * restart lifecycle, and the strip_history.txt/strip_state.txt/
 * strip_frame_changed.txt file-relay contract.
 *
 * Owns: the Xlib windows, ALL drawing (via the layout engine's render
 * walk), ALL hit-testing (via the SAME rectangles the render walk itself
 * computed — single source of truth, no draw/hit-test drift), and the
 * manager child process.
 *
 * Does NOT own KtbState — this binary never touches livedesk_open.txt,
 * nav claims, shortcuts.pdl, theme.pdl etc. directly. It only reads
 * strip_state.txt + the three strip_var_*.txt markup-fragment files
 * (written by khtpm_taskbar_manager_main.c) and writes strip_history.txt
 * (polled by that same binary).
 */
#ifndef _WIN32
#define _DEFAULT_SOURCE /* strtok_r()/kill() under -std=c11 strict mode, matches
                          * khtpm_taskbar_manager.c's own convention for the same issue */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h> /* REAL FIX 2026-08-13 (direct live report:
 * "used to show chinese but theres a bug" - the datetime cell showed
 * garbled glyphs, not Chinese text): every draw call in this file used
 * plain XDrawString(), the legacy X11 8-bit/Latin-1 text function -
 * it treats each byte of a multi-byte UTF-8 string (e.g. Chinese
 * "2026年08月13日") as a SEPARATE Latin-1 codepoint, producing garbage,
 * not tofu/missing-glyph boxes. khtpm_hq_render.c (same ops/ dir,
 * db-hq's own renderer) already solves this correctly with
 * XftDrawStringUtf8 - ported that exact pattern here rather than
 * inventing a new one. See font_for_strip()/xft_color_strip() below. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>

#include "khtpm_taskbar_manager.h" /* KTB_* constants only (window sizing, tab/shortcut caps) */
#include "khtpm_strip_codes.h"
#include "khtpm_strip_layout.h"

#define SP_PATH_BUF KTB_PATH_BUF

/* Matches tp_taskbar.c's own POLL_INTERVAL_USEC (300ms) for consistency
 * across all taskbar-family binaries, not an independently chosen value. */
#define POLL_INTERVAL_USEC 300000

/* ---------------------------------------------------------------------
 * strip_state.txt / strip_var_*.txt reader.
 *
 * Necessitation resolved (SCOPE.md's own open item, "theme colors ...
 * decide at implementation time, lean toward parser reads its own
 * rendering config directly"): theme_bg/theme_fg stay a DIRECT scalar
 * read out of strip_state.txt's own KEY rows (same as before this pass),
 * NOT routed through the ${var} layout-substitution system — nothing in
 * either .chtpm file references ${theme_bg}/${theme_fg}, matching
 * load_theme_opacity()'s own precedent of reading #.desktop/
 * livedesk_theme.pdl directly. The layout engine's get_var() callback
 * below only ever serves the FOUR real markup/label vars the two layout
 * files actually use: strip_tabs, strip_shortcuts, strip_hq_items,
 * cliio_label.
 *
 * Multi-line VAR strategy (SCOPE.md necessitation, resolved in
 * khtpm_taskbar_manager_main.c per that file's own comment): one small
 * file per var (strip_var_tabs.txt / strip_var_shortcuts.txt /
 * strip_var_hqitems.txt), read whole here, matching the manager's write
 * side exactly.
 * ------------------------------------------------------------------- */
typedef struct {
    char theme_bg[32];
    char theme_fg[32];
    char digit_buf[16];
    int  nav_armed;
    int  hq_focus; /* manager's own LOCAL submenu-row cursor (ktb_hq_digit()/
                     * ktb_hq_focus_delta()) — real gap fix, 2026-08-11, see
                     * sync_popup_focus() below for why the parser needs this. */
    int  hq_open;  /* manager's own real "is a menu open, and which one"
                     * state (0 = closed) — real gap fix, 2026-08-11: the
                     * parser used to ASSUME every submenu row selection
                     * closes the menu (wrong for "load", which replaces
                     * the content in place instead); this is the real,
                     * authoritative signal for when to actually close. */
    int  cliio_active;
    int  cliio_typing;
    char cliio_op[32];
    char cliio_buffer[256];
    char cliio_label[64];

    char var_tabs[48 * 1024];
    char var_shortcuts[48 * 1024];
    char var_hqitems[48 * 1024];

    /* Real gap fix (2026-08-11, direct request: "the button vars for
     * user/file/desk") — live USER/file/desks labels, published by the
     * manager as their own small files, same pattern as the three vars
     * above. */
    char var_username[128];
    char var_file_label[256];
    char var_desks_label[64];
    char var_avatar_dir[SP_PATH_BUF]; /* real house-root-prefixed path, not a short label */
    char var_datetime[128]; /* live formatted date/time, published by manager */
} SpState;

static char g_house_root[SP_PATH_BUF];
static pid_t g_manager_pid = -1;
static SpState g_st;

/* Real X11 keyboard focus tracking (2026-08-11, direct request: "does it
 * have focus? right click doesn't say... lets add a '^' in nav section if
 * it has focus, for sanity"). taskbar_soft_focus()'s XSetInputFocus() call
 * is a REQUEST, not a guarantee — this tracks whether the window ACTUALLY
 * has focus right now via real FocusIn/FocusOut events, the only
 * authoritative source. Doubles as the actual diagnostic for "arrows/
 * digits don't move the cursor": if g_has_real_focus never goes true,
 * KeyPress events never reach this process at all, which would fully
 * explain that symptom without needing a second investigation. */
static int g_has_real_focus = 0;

static void path_join2(char *out, size_t n, const char *root, const char *rel) {
    size_t rl = strlen(root);
    if (rl > 0 && (root[rl - 1] == '/' || root[rl - 1] == '\\'))
        snprintf(out, n, "%s%s", root, rel);
    else
        snprintf(out, n, "%s/%s", root, rel);
}

static void read_small_file(const char *rel_path, char *out, size_t outsz) {
    out[0] = '\0';
    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, rel_path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    size_t n = fread(out, 1, outsz - 1, f);
    out[n] = '\0';
    fclose(f);
}

static char *trim_field(char *s) {
    while (*s == ' ') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
    return s;
}

static void load_state(SpState *st) {
    memset(st, 0, sizeof(*st));
    snprintf(st->theme_bg, sizeof(st->theme_bg), "white");
    snprintf(st->theme_fg, sizeof(st->theme_fg), "black");

    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_state.txt");
    FILE *f = fopen(path, "r");
    if (f) {
        char line[SP_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *save = NULL;
            char *col0 = strtok_r(line, "|", &save);
            if (!col0) continue;
            col0 = trim_field(col0);
            if (strcmp(col0, "KEY") != 0) continue; /* TAB/SHORTCUT/HQITEM rows kept for debug only, unused here */
            char *keyf = strtok_r(NULL, "|", &save);
            char *valf = strtok_r(NULL, "\n", &save);
            if (!keyf || !valf) continue;
            char *key = trim_field(keyf);
            char *val = trim_field(valf);
            if (strcmp(key, "theme_bg") == 0) snprintf(st->theme_bg, sizeof(st->theme_bg), "%s", val);
            else if (strcmp(key, "theme_fg") == 0) snprintf(st->theme_fg, sizeof(st->theme_fg), "%s", val);
            else if (strcmp(key, "digit_buf") == 0) snprintf(st->digit_buf, sizeof(st->digit_buf), "%s", val);
            else if (strcmp(key, "nav_armed") == 0) st->nav_armed = atoi(val);
            else if (strcmp(key, "hq_focus") == 0) st->hq_focus = atoi(val);
            else if (strcmp(key, "hq_open") == 0) st->hq_open = atoi(val);
            else if (strcmp(key, "cliio_active") == 0) st->cliio_active = atoi(val);
            else if (strcmp(key, "cliio_typing") == 0) st->cliio_typing = atoi(val);
            else if (strcmp(key, "cliio_op") == 0) snprintf(st->cliio_op, sizeof(st->cliio_op), "%s", val);
            else if (strcmp(key, "cliio_buffer") == 0) snprintf(st->cliio_buffer, sizeof(st->cliio_buffer), "%s", val);
            else if (strcmp(key, "cliio_label") == 0) snprintf(st->cliio_label, sizeof(st->cliio_label), "%s", val);
        }
        fclose(f);
    }

    read_small_file("#.desktop/strip_var_tabs.txt", st->var_tabs, sizeof(st->var_tabs));
    read_small_file("#.desktop/strip_var_shortcuts.txt", st->var_shortcuts, sizeof(st->var_shortcuts));
    read_small_file("#.desktop/strip_var_hqitems.txt", st->var_hqitems, sizeof(st->var_hqitems));
    read_small_file("#.desktop/strip_var_username.txt", st->var_username, sizeof(st->var_username));
    read_small_file("#.desktop/strip_var_file_label.txt", st->var_file_label, sizeof(st->var_file_label));
    read_small_file("#.desktop/strip_var_desks_label.txt", st->var_desks_label, sizeof(st->var_desks_label));
    read_small_file("#.desktop/strip_var_avatar_dir.txt", st->var_avatar_dir, sizeof(st->var_avatar_dir));
    read_small_file("#.desktop/strip_var_datetime.txt", st->var_datetime, sizeof(st->var_datetime));
}

/* The layout engine's ${var} lookup, backed by the vars the two .chtpm
 * files actually reference. Must never return NULL, matching
 * LayVarLookupFn's contract. */
static const char *sp_get_var(const char *name, void *ctx) {
    SpState *st = (SpState *)ctx;
    if (strcmp(name, "strip_tabs") == 0) return st->var_tabs;
    if (strcmp(name, "strip_shortcuts") == 0) return st->var_shortcuts;
    if (strcmp(name, "strip_hq_items") == 0) return st->var_hqitems;
    if (strcmp(name, "cliio_label") == 0) return st->cliio_label;
    if (strcmp(name, "username") == 0) return st->var_username;
    if (strcmp(name, "file_label") == 0) return st->var_file_label;
    if (strcmp(name, "desks_label") == 0) return st->var_desks_label;
    if (strcmp(name, "avatar_dir") == 0) return st->var_avatar_dir;
    if (strcmp(name, "datetime") == 0) return st->var_datetime;
    return "";
}

/* ---------------------------------------------------------------------
 * Manager child process lifecycle — UNCHANGED from the prior pass, mirrors
 * chtpm_parser.c's launch_module() (fork+execv, tracked pid) and
 * cleanup_module() (SIGTERM on shutdown). "Lazy restart" per design §5:
 * only re-launched the next time an input event needs it.
 * ------------------------------------------------------------------- */
static void launch_manager(void) {
    char exe[SP_PATH_BUF];
    /* Real fix, 2026-08-11: hardcoded the "_test"-suffixed name from
     * before legacy tp_taskbar.c was retired — the manager binary was
     * renamed (dropped "_test") once khtpm became the real, only
     * taskbar, but this specific hardcoded path was missed in that pass.
     * Live-caught: the manager silently failed to fork (execl() against
     * a binary that no longer existed), so digit/Enter input reached the
     * parser but never got forwarded anywhere. */
    path_join2(exe, sizeof(exe), g_house_root,
               "*.monads/*.livedesk-taskbar/ops/+x/khtpm_taskbar_manager_main.+x");

    pid_t pid = fork();
    if (pid == 0) {
        execl(exe, exe, g_house_root, (char *)NULL);
        _exit(1); /* execl failed */
    } else if (pid > 0) {
        g_manager_pid = pid;
    } else {
        fprintf(stderr, "strip_parser: fork() failed: %s\n", strerror(errno));
    }
}

static int ensure_manager_running(void) {
    if (g_manager_pid <= 0) { launch_manager(); return 1; }
    return 0;
}

static void reap_manager_nonblocking(void) {
    if (g_manager_pid <= 0) return;
    int status;
    pid_t r = waitpid(g_manager_pid, &status, WNOHANG);
    if (r == g_manager_pid) {
        g_manager_pid = -1; /* dead — lazily relaunched on next input, not here */
    }
}

/* Real bug fix (2026-08-11, direct live report: "entries 11 and 12 on bar
 * are getting cutt off" — a regression from the USER/file/desks var
 * change): launch_manager() forks the manager and returns immediately;
 * the freshly-exec'd child needs real startup time (ktb_init/ktb_reload,
 * then its own first publish_state()) before strip_var_username.txt etc.
 * actually exist with real content. The very next thing main() used to do
 * was call load_state() with zero wait, then compute hq_win's PERMANENT
 * width from whatever that (likely still-empty) state was — a real race,
 * not merely a hypothetical one, confirmed by the observed symptom
 * matching exactly: header_total_width() under-measured USER/file/desks
 * before their real (longer) values had ever been read once. Wait for the
 * manager's own first real publish (strip_state.txt existing with
 * non-trivial content) before proceeding, bounded so a slow/dead manager
 * can't hang the parser forever — 1s cap, matches this being a one-time
 * startup cost, not a per-frame one. */
static void wait_for_manager_first_publish(void) {
    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_state.txt");
    for (int i = 0; i < 40; i++) { /* 40 * 25ms = up to 1s */
        struct stat st;
        if (stat(path, &st) == 0 && st.st_size > 0) return;
        struct timespec ts = { 0, 25 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
}

static void cleanup_manager(void) {
    if (g_manager_pid > 0) {
        kill(g_manager_pid, SIGTERM);
        waitpid(g_manager_pid, NULL, WNOHANG);
        g_manager_pid = -1;
    }
}

static volatile sig_atomic_t g_running = 1;
static void on_sigterm(int sig) { (void)sig; g_running = 0; }

/* Cheap size-check dirty poll of strip_frame_changed.txt — UNCHANGED. */
static long g_frame_changed_cursor = -1;

static int frame_changed_dirty(void) {
    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_frame_changed.txt");
    struct stat st;
    if (stat(path, &st) != 0) return g_frame_changed_cursor != 0 ? (g_frame_changed_cursor = 0, 1) : 0;
    if (g_frame_changed_cursor < 0) { g_frame_changed_cursor = st.st_size; return 1; /* first sight: draw once */ }
    if (st.st_size != g_frame_changed_cursor) { g_frame_changed_cursor = st.st_size; return 1; }
    return 0;
}

/* Append a resolved decimal action code to strip_history.txt — UNCHANGED. */
static void send_code(int code) {
    if (code <= 0) return;
    int just_launched = ensure_manager_running();
    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/strip_history.txt");
    if (just_launched) usleep(150000);
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%d\n", code); fclose(f); }
}

static unsigned long parse_color(Display *dpy, const char *name, unsigned long fallback) {
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor c;
    if (XAllocNamedColor(dpy, cmap, name, &c, &c)) return c.pixel;
    return fallback;
}

static void set_window_opacity(Display *dpy, Window w, double opacity) {
    if (opacity < 0.0) opacity = 0.0;
    if (opacity > 1.0) opacity = 1.0;
    Atom opacity_atom = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long val = (unsigned long)(opacity * (double)0xFFFFFFFFUL);
    XChangeProperty(dpy, w, opacity_atom, XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *)&val, 1);
}

static double load_theme_opacity(void) {
    double opacity = 0.5;
    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/livedesk_theme.pdl");
    FILE *f = fopen(path, "r");
    if (!f) return opacity;
    char line[SP_PATH_BUF];
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
        if (v[0] == '\0') continue;
        double parsed = atof(v);
        if (parsed >= 0.0 && parsed <= 1.0) opacity = parsed;
    }
    fclose(f);
    return opacity;
}

static void load_strip_offset(int *out_x, int *out_y) {
    *out_x = 0;
    *out_y = 40;
    char path[SP_PATH_BUF];
    path_join2(path, sizeof(path), g_house_root, "#.desktop/livedesk_taskbar.pdl");
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[SP_PATH_BUF];
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
        if (strcmp(key, "strip_x_offset") == 0) *out_x = atoi(v);
        else if (strcmp(key, "strip_y_offset") == 0) *out_y = atoi(v);
    }
    fclose(f);
}

static void taskbar_set_wm_class(Display *dpy, Window w) {
    XClassHint *ch = XAllocClassHint();
    if (!ch) return;
    ch->res_name = (char *)"MuchiverseLivedesk";
    ch->res_class = (char *)"MuchiverseLivedesk";
    XSetClassHint(dpy, w, ch);
    XFree(ch);
}

/* Real, permanent frame-history log (2026-08-11, direct request: "chtpm
 * does a frame history everytime frame updates. we should do the same
 * (and refresh on new session or 256kb limit"). Real CHTPM's own habit
 * (chtpm_parser.c, multiple call sites) is a simple always-append
 * "debug.txt" — confirmed by direct read, no size-cap logic exists there
 * at all. The 256KB-cap-and-refresh-per-session behavior is this house's
 * own separate, real convention (tp_taskbar.c's check_log_size()/
 * LOG_MAX_BYTES, 262144 bytes) — combined here per direct instruction:
 * CHTPM's append-every-update HABIT, this house's own size/refresh
 * discipline. Lives in a real location (#.desktop/), not /tmp, so it
 * persists like every other taskbar-family log. */
#define STRIP_FRAME_HISTORY_MAX_BYTES 262144
static char g_frame_history_path[SP_PATH_BUF];

static void frame_history_init(void) {
    path_join2(g_frame_history_path, sizeof(g_frame_history_path), g_house_root,
               "#.desktop/khtpm_strip_frame_history.txt");
    /* "refresh on new session": truncate fresh at process startup, not
     * accumulated across restarts. */
    FILE *f = fopen(g_frame_history_path, "w");
    if (f) fclose(f);
}

static void append_frame_history(const LayDoc *header_doc, const LayDoc *bottom_doc, int nav_focus) {
    if (!g_frame_history_path[0]) return;
    struct stat st;
    if (stat(g_frame_history_path, &st) == 0 && st.st_size > STRIP_FRAME_HISTORY_MAX_BYTES)
        truncate(g_frame_history_path, 0);
    FILE *f = fopen(g_frame_history_path, "a");
    if (!f) return;
    const char *hf_type = "-", *hf_label = "-", *hf_onclick = "-";
    if (header_doc->focus_index >= 0 && header_doc->focus_index < header_doc->element_count) {
        hf_type = header_doc->elements[header_doc->focus_index].type;
        hf_label = header_doc->elements[header_doc->focus_index].label;
        hf_onclick = header_doc->elements[header_doc->focus_index].onClick;
    }
    fprintf(f, "header.focus=%d[type=%s label=%s onClick=%s] header.active=%d "
               "bottom.focus=%d unified_nav_focus=%d has_real_x11_focus=%d "
               "cliio_active=%d nav_armed=%d digit_buf=%s hq_focus=%d element_count=%d\n",
            header_doc->focus_index, hf_type, hf_label, hf_onclick, header_doc->active_index,
            bottom_doc->focus_index, nav_focus, g_has_real_focus,
            g_st.cliio_active, g_st.nav_armed, g_st.digit_buf, g_st.hq_focus, header_doc->element_count);
    fclose(f);
}

static void taskbar_soft_focus(Display *dpy, Window w) {
    if (!w) return;
    XRaiseWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
    XFlush(dpy);
}

/* Real architecture fix (2026-08-11, direct correction: "focus is for
 * both. do u understand? there is no distinction between top and bottom.
 * treat them as one. if i click bottom i should beable to move top navs
 * (this was present in legacy)"). A prior pass here tracked header_doc
 * and bottom_doc's focus_index as two independent variables with a
 * bottom_has_real_focus flag deciding which one "won" — structurally the
 * exact same "two cursors" bug tp_taskbar.c's own nav_focus_step()/
 * nav_focus_apply() header comment (~line 1500) documents fixing in
 * legacy, for the same reason: "two separately-tracked cursor variables
 * per widget... could desync." Ported the real fix instead of
 * re-inventing a worse one: ONE unified index (g_nav_focus) spans
 * [header cells 0..hc-1][tabs hc..hc+tc-1], exactly like legacy's own
 * [strip buttons][tabs] layout — header_doc/bottom_doc's own focus_index
 * fields become pure DERIVED display state, written ONLY by
 * unified_apply() below, never independently again. Arrow keys step this
 * one index (unified_step(), matches nav_focus_step() including its
 * wraparound + skip-non-navigable behavior); mouse clicks on either
 * window also update it so a later arrow press continues naturally from
 * wherever was just clicked (matches legacy's own nav_focus sync on
 * click/digit-jump). This only governs TOP-LEVEL navigation (no submenu
 * open) — while a header cell's ACTIVATE scope is open, arrow keys still
 * use lay_focus_delta()'s existing LOCAL behavior unchanged, matching
 * legacy's own separate g_hq_popup_open arrow-handling branch. */
static int g_nav_focus = 0;

/* Real bug fix (2026-08-11, direct live report: "nav is supposed to go
 * from 12 to 13, and from 18 to 1 and 1 to 18... currently its stuck up
 * at 1-12"): these three helpers originally checked literal
 * `parent_index != -1` (true root-of-panel children only) — but
 * khtpm_strip_bottom.chtpm's real structure wraps its tab buttons one
 * level deeper, inside `<row>${strip_tabs}</row>` (confirmed by reading
 * both the layout file and the manager's actual strip_var_tabs.txt
 * output: `<row><button label="13. self" onClick="TAB:0"/>...`), so every
 * tab's parent_index pointed at the <row>, not the panel — root_nav_count
 * on bottom_doc was silently always 0, capping unified nav at the header's
 * 12 cells. Fixed by using lay_is_navigable() (already the correct,
 * general, any-depth predicate — ported from real CHTPM's own
 * is_navigable(), it already handles "not nested under a closed ACTIVATE
 * scope" correctly at any nesting depth) instead of a hand-rolled
 * root-only check. Safe to call unguarded here because every call site
 * already only invokes these while header_doc.active_index == -1 (see
 * this file's own g_nav_focus comment block). */
static int root_nav_count(const LayDoc *doc) {
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (!lay_is_navigable(doc, i)) continue;
        n++;
    }
    return n;
}

static int root_nav_element_at(const LayDoc *doc, int pos) {
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (!lay_is_navigable(doc, i)) continue;
        if (n == pos) return i;
        n++;
    }
    return -1;
}

static int root_nav_pos_of(const LayDoc *doc, int elidx) {
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (!lay_is_navigable(doc, i)) continue;
        if (i == elidx) return n;
        n++;
    }
    return -1;
}

/* Single place that ever writes header_doc->focus_index/bottom_doc->
 * focus_index from the unified g_nav_focus — mirrors tp_taskbar.c's own
 * nav_focus_apply() comment exactly: "keeps the two derived variables
 * from ever disagreeing about which one thing is actually focused." */
static void unified_apply(LayDoc *header_doc, LayDoc *bottom_doc) {
    int hc = root_nav_count(header_doc);
    if (g_nav_focus >= 0 && g_nav_focus < hc) {
        header_doc->focus_index = root_nav_element_at(header_doc, g_nav_focus);
        bottom_doc->focus_index = -1;
    } else {
        header_doc->focus_index = -1;
        int bc = root_nav_count(bottom_doc);
        int bpos = g_nav_focus - hc;
        bottom_doc->focus_index = (bpos >= 0 && bpos < bc) ? root_nav_element_at(bottom_doc, bpos) : -1;
    }
}

static void unified_step(LayDoc *header_doc, LayDoc *bottom_doc, int delta) {
    int total = root_nav_count(header_doc) + root_nav_count(bottom_doc);
    if (total <= 0) return;
    g_nav_focus = ((g_nav_focus + delta) % total + total) % total;
    unified_apply(header_doc, bottom_doc);
}

/* Real bug fix (2026-08-11, direct live report: "focus should go to 1
 * always like chtpm... also right clicking isn't giving taskbar focus
 * yet"): right-click used to (pre-layout-rewrite) send KSC_NAV_ARM to the
 * manager, which set the MANAGER's own nav_armed/strip_focus_cell — but
 * since this rewrite, the visible [>] cursor is driven by the PARSER's
 * own LOCAL doc->focus_index (the design correction in SCOPE.md/
 * khtpm_strip_layout.h), which right-click never touched at all. So
 * XSetInputFocus was genuinely being called (real X11 focus WAS granted)
 * but nothing visible changed, which is exactly why it read as "not
 * giving focus" — there was no feedback. Matches real CHTPM's own
 * initialize_focus() (called after any external re-arm event, same
 * pattern as this file's own lay_load()): reset to the FIRST navigable
 * element, always index/position 1, not "wherever it happened to be." */
static void lay_focus_first(LayDoc *doc) {
    for (int i = 0; i < doc->element_count; i++) {
        if (lay_is_navigable(doc, i)) { doc->focus_index = i; return; }
    }
}

/* Real bug fix (2026-08-11, direct live report: "arrows working but not
 * digits" — confirmed as an already-known, documented gap from the
 * original parser pass: digits reach the MANAGER (updating digit_buf/
 * nav_armed, visible in the [NAV] box) but nothing ever read that back to
 * move the PARSER's own local cursor, since digit-jump validation
 * (max_claimed_nav/cap/restart-on-invalid, all real manager-owned
 * business logic fixed earlier this same session) deliberately stays in
 * the manager, not duplicated here).
 *
 * UPDATED (2026-08-11, same day): originally scoped to header cells
 * (1-12) only, matching digit_buf against each header <button>'s onClick
 * ":n" suffix by string — tab digit-jump (13+) was left as a documented,
 * not-yet-wired gap because tab onClick values are "TAB:<0-based index
 * into the tab list>", not a unified 1-based nav number, so the same
 * suffix-match approach doesn't extend to bottom_doc at all.
 *
 * Fixed by switching to POSITION-based indexing instead of suffix-
 * matching: g_nav_focus already spans [header 0..hc-1][tabs hc..hc+tc-1]
 * in exact document order (unified_apply()'s own contract), and
 * sync_strip_claims()'s NAV=i+1 numbering (manager-side) already assigns
 * digit_buf's nav numbers in that same position order for BOTH header
 * cells and tabs — so nav_n-1 is directly g_nav_focus, unconditionally,
 * with no per-element string matching needed at all. This is simpler than
 * the suffix-match it replaces and covers tabs "for free" since
 * unified_apply() already knows how to split a position between the two
 * docs. */
static void sync_focus_to_digit_buf(LayDoc *header_doc, LayDoc *bottom_doc, const SpState *st) {
    if (!st->digit_buf[0]) return;
    /* Real bug fix (2026-08-11, found while live-verifying the new
     * player>reset submenu row-select: sync_popup_focus()'s correct row
     * pick was silently getting clobbered back to -1 immediately after).
     * digit_buf is the TOP-LEVEL nav-arm buffer and stays set to whatever
     * cell number opened the current submenu (e.g. "8" for player) even
     * after a header ACTIVATE scope opens — it's a separate accumulator
     * from the manager's own hq_digit_accum/hq_focus (see ktb_hq_digit()),
     * not cleared just because a submenu is now open. unified_apply()
     * only makes sense for TOP-LEVEL (no submenu) focus — while active_index
     * != -1, lay_is_navigable() is scoped to just the open menu's own
     * descendants, so root_nav_element_at() resolving the STALE top-level
     * digit_buf position against that much smaller navigable set silently
     * returns -1, overwriting sync_popup_focus()'s correct in-scope pick.
     * Guarded the same way the OTHER unified_apply() call site already is. */
    if (header_doc->active_index != -1) return;
    int nav_n = atoi(st->digit_buf);
    if (nav_n <= 0) return;
    int total = root_nav_count(header_doc) + root_nav_count(bottom_doc);
    if (nav_n > total) return;
    g_nav_focus = nav_n - 1;
    unified_apply(header_doc, bottom_doc);
}

/* ---------------------------------------------------------------------
 * onClick dispatch — replaces the old bare-decimal KSC_HQ_HEADER_BASE/
 * KSC_HQ_ITEM_BASE range dispatch with resolution through the tag tree's
 * own onClick attribute string, matching real CHTPM's send_command() model
 * (chtpm_parser.c ~line 1550): some onClick values are pure LOCAL scope
 * changes (ACTIVATE/BACK — no manager round-trip at all, per SCOPE.md's
 * explicit instruction), others resolve to a manager-bound decimal code
 * appended to strip_history.txt (KEY:-equivalent, this strip's own
 * TAB:/SHORTCUT:/STRIP:/HQITEM: prefixes standing in for CHTPM's KEY:n).
 * ------------------------------------------------------------------- */
static void dispatch_onclick(LayDoc *doc, int idx) {
    if (idx < 0 || idx >= doc->element_count) return;
    const char *oc = doc->elements[idx].onClick;
    if (!oc[0]) return;

    if (lay_is_activate_marker(oc)) {
        lay_activate(doc, idx);
        const char *colon = strchr(oc, ':');
        if (colon) {
            int n = atoi(colon + 1);
            if (n > 0) send_code(KSC_HQ_HEADER_BASE + n);
        }
        return;
    }
    if (strcmp(oc, "BACK") == 0) {
        lay_back(doc);
        send_code(KSC_ESCAPE);
        return;
    }
    if (strncmp(oc, "STRIP:", 6) == 0) {
        int n = atoi(oc + 6);
        if (n > 0) send_code(KSC_HQ_HEADER_BASE + n);
        return;
    }
    if (strncmp(oc, "TAB:", 4) == 0) {
        int i = atoi(oc + 4);
        send_code(KSC_TAB_BASE + i);
        return;
    }
    if (strncmp(oc, "SHORTCUT:", 9) == 0) {
        int i = atoi(oc + 9);
        if (i >= 0 && i < KTB_MAX_SHORTCUTS) send_code(KSC_SHORTCUT_BASE + i);
        return;
    }
    if (strncmp(oc, "HQITEM:", 7) == 0) {
        int i = atoi(oc + 7);
        if (i >= 0 && i < KTB_LIVEDESK_DYN_MAX) send_code(KSC_HQ_ITEM_BASE + i);
        /* Real bug fix (2026-08-11, found live during an actual save/
         * load harness test: selecting "load" opened a blank-looking
         * menu — the manager correctly published a real 7-session picker
         * (confirmed via strip_state.txt/strip_var_hqitems.txt) but
         * nothing was navigable locally). Root cause: this used to
         * unconditionally lay_back(doc) after EVERY HQITEM: row,
         * wrongly assuming every row command closes the menu — true for
         * "save"/"new-desk"/most rows (manager calls ktb_hq_close(),
         * hq_open=0), but NOT for "load", which calls ktb_hq_open(s, 13)
         * instead (khtpm_taskbar_manager.c ~line 1968, its own comment:
         * "replaces the current row list in place with the session
         * picker") — hq_open STAYS truthy (now 13, the internal-only
         * picker code), it never goes to 0. Fixed: no longer lay_back()
         * here at all — sync_popup_focus()'s own tick (see that
         * function, extended below) now closes the local scope only when
         * the MANAGER's real hq_open state says nothing is open anymore,
         * instead of assuming from the onClick prefix alone. */
        return;
    }
}

/* ---------------------------------------------------------------------
 * AGENT RELAY (2026-08-11, direct correction: "did u not add this as
 * legacy had?" — a real gap, this was missed during the port). Ported
 * from tp_taskbar.c's own poll_agent_relay()/agent_relay_dispatch() (read
 * in full, ~line 3195-3400, before writing this — see that function's own
 * big header comment and _.0.aigent-testing-k9.txt for the full contract
 * this must preserve): a plain file-based input channel,
 * #.desktop/livedesk_agent_relay.txt, one decimal ASCII code per line
 * (48-57 digits, 13 Enter, 27 Escape, 8 Backspace, 32-126 other printable
 * — NO arrow-key codes, digit-based nav only, matching legacy's own
 * contract exactly), so an agent (or a second human) can drive this
 * taskbar with zero X11 involvement — no XTest, no XSendEvent, no shared
 * input focus with a real human using the same display at the same time.
 *
 * ARCHITECTURAL DEVIATION FROM LEGACY, DELIBERATE: tp_taskbar.c's own
 * agent_relay_dispatch() is a documented, intentional DUPLICATE of its
 * inline KeyPress handler's logic — done that way specifically so adding
 * agent support could never regress the already-verified real-keyboard
 * path (see that function's own "KNOWN DUPLICATION" comment). That
 * tradeoff made sense for tp_taskbar.c's architecture, where nav_armed/
 * digit_buf/cli-io state is a dense local state machine duplicated by
 * hand. It does NOT make sense here: khtpm's architecture already
 * delegates nearly everything (digit accumulation, nav arming, cli-io
 * typing) to the MANAGER via send_code() — the real KeyPress handler's
 * own Enter/Escape/Backspace/printable branches are already thin
 * forwarding calls, not a state machine to protect from regression. So
 * this port SHARES dispatch_key_code() between the real KeyPress handler
 * and the relay poller below instead of duplicating it — less code, and
 * an agent-injected code gets IDENTICAL handling to a real keypress by
 * construction, not "kept in sync by hand." Arrow-key nav (unified_step())
 * stays real-X11-only in the event loop, matching legacy's own relay
 * contract having no arrow support either — digits are how the relay
 * navigates, exactly like nav.sh's own arm()+type_digits() sequence.
 * ------------------------------------------------------------------- */
static void dispatch_key_code(LayDoc *header_doc, LayDoc *bottom_doc, const SpState *st, int code) {
    if (code == KSC_ENTER) {
        if (st->cliio_active) {
            send_code(KSC_ENTER); /* cli_io typing start/submit — manager-owned */
        } else if (header_doc->active_index != -1) {
            /* Real bug fix (2026-08-11, found live-testing player>reset via
             * the agent relay): an open header submenu must take priority
             * over bottom_doc entirely — bottom_doc->focus_index can hold a
             * perfectly valid, navigable, but STALE value left over from
             * before the submenu opened (unified_apply() deliberately
             * stops touching either doc while active_index != -1, so
             * nothing ever clears it), which the old unconditional
             * "check bottom first" order would wrongly activate instead of
             * the submenu row sync_popup_focus() just moved the cursor to.
             * Matches the manager's own dispatch_code() precedence (its
             * hq_open branch is checked before the bottom-tab digit logic,
             * same idea). */
            dispatch_onclick(header_doc, header_doc->focus_index);
        } else if (bottom_doc->focus_index >= 0 && lay_is_navigable(bottom_doc, bottom_doc->focus_index)) {
            dispatch_onclick(bottom_doc, bottom_doc->focus_index);
        } else if (lay_is_navigable(header_doc, header_doc->focus_index)) {
            dispatch_onclick(header_doc, header_doc->focus_index);
        }
    } else if (code == KSC_ESCAPE) {
        send_code(KSC_ESCAPE); /* manager: closes cliio, or hq_open, or clears digit_buf */
        if (header_doc->active_index != -1 &&
            strcmp(header_doc->elements[header_doc->active_index].type, "cli_io") != 0) {
            lay_back(header_doc);
        }
    } else if (code == KSC_BACKSPACE) {
        send_code(KSC_BACKSPACE);
    } else if (code >= 0x20 && code < 0x7f) {
        send_code(code);
    }
}

static long g_relay_cursor = -1; /* -1 = uninitialized; set to EOF on first poll (never replay backlog) */

static void livedesk_relay_path(const char *house_root, char *out, size_t sz) {
    snprintf(out, sz, "%s/#.desktop/livedesk_agent_relay.txt", house_root);
}

/* Poll the relay file for new complete lines since last read. Cheap
 * short-circuit via file size, safe to call on every ~300ms loop tick.
 * Cursor/truncation-resync discipline matches tp_taskbar.c's own
 * poll_agent_relay() exactly: never replay backlog on first call, resync
 * (don't replay) on truncation, leave a partial trailing line for the
 * next poll rather than consuming it. Returns the number of codes
 * dispatched so the caller knows whether a redraw is needed. */
static int poll_agent_relay(const char *house_root, LayDoc *header_doc, LayDoc *bottom_doc, const SpState *st) {
    char path[SP_PATH_BUF];
    livedesk_relay_path(house_root, path, sizeof(path));
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
        if (!nl) break; /* partial line at EOF — wait for the rest next poll */
        *nl = '\0';
        long here = ftell(f);
        int code = atoi(line);
        if (code > 0) { dispatch_key_code(header_doc, bottom_doc, st, code); n_dispatched++; }
        consumed = here;
    }
    fclose(f);
    g_relay_cursor = consumed;
    return n_dispatched;
}

/* ---------------------------------------------------------------------
 * Render walk — replaces draw_header_win()/draw_popup_win()/the old
 * draw_strip() tab loop. Draws via the SAME Xlib primitives already in
 * use (offscreen pixmap + XCopyArea, per SCOPE.md's explicit "keep this,
 * it's proven working" instruction), and computes the cursor prefix
 * itself from focus_index/active_index via lay_cursor_prefix() — the
 * real design correction from SCOPE.md's "Cursor/focus rendering"
 * section: nothing is baked into any label string by the manager.
 * ------------------------------------------------------------------- */
#define SP_MAX_HIT 96
typedef struct {
    int x0, y0, x1, y1;
    int elidx;       /* index into whichever LayDoc owns this rect */
} SpHitRect;

static SpHitRect g_header_hits[SP_MAX_HIT];
static int g_header_hit_n = 0;
static int g_header_row_w = 0;

static SpHitRect g_popup_hits[SP_MAX_HIT];
static int g_popup_hit_n = 0;

static SpHitRect g_bottom_hits[SP_MAX_HIT];
static int g_bottom_hit_n = 0;

#define STRIP_NAV_BOX_W 64

/* ---------- Xft/UTF8 text (real fix 2026-08-13, see this file's own
 * top-of-file comment on the #include <X11/Xft/Xft.h> line for the
 * full incident). One shared XftDraw context per offscreen Pixmap
 * (g_hq_buf/g_popup_buf/g_strip_buf), recreated alongside each
 * Pixmap's own recreate-on-resize block - same lifecycle, same
 * pattern khtpm_hq_render.c already proves works. One shared font
 * (Noto Sans CJK SC - confirmed present on this house's real desktop
 * via `fc-match`, covers Latin + CJK in one family so callers never
 * need to pick a font per-string) rather than per-element font_for()
 * (this file has no CSS styling concept to key off, unlike db-hq). */
static Colormap g_strip_cmap = 0;
static XftFont *g_strip_font = NULL;
static XftDraw *g_hq_xft = NULL, *g_popup_xft = NULL, *g_strip_xft = NULL;

static XftFont *strip_font(Display *dpy, int screen) {
    if (g_strip_font) return g_strip_font;
    g_strip_font = XftFontOpenName(dpy, screen, "Noto Sans CJK SC:pixelsize=13");
    if (!g_strip_font) g_strip_font = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=13");
    return g_strip_font;
}

static XftColor strip_xft_color(Display *dpy, int screen, unsigned long pixel) {
    /* Converts an already-allocated X11 pixel value (this file works
     * in raw pixel values throughout, not #rrggbb strings) to an
     * XftColor by round-tripping through XQueryColor - avoids
     * duplicating this file's own pixel-allocation scheme. */
    XftColor xc;
    XColor qc; qc.pixel = pixel;
    XQueryColor(dpy, g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, screen), &qc);
    XRenderColor rc = { qc.red, qc.green, qc.blue, 0xffff };
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, screen), &rc, &xc);
    return xc;
}

/* Draws UTF-8 text at the same (x, baseline-y) convention the old
 * XDrawString call sites used, onto whichever XftDraw context matches
 * the target Pixmap (buf). fg_pixel is the X11 pixel value the
 * caller's GC was already set to (XGetGCValues(GCForeground)) - reuse
 * it rather than introducing a second, separate color scheme. */
static void strip_draw_utf8(Display *dpy, int screen, XftDraw *xftd, unsigned long fg_pixel,
                             int x, int y, const char *s, int len) {
    if (!xftd || !s || len <= 0) return;
    XftFont *f = strip_font(dpy, screen);
    if (!f) return;
    XftColor col = strip_xft_color(dpy, screen, fg_pixel);
    XftDrawStringUtf8(xftd, &col, f, x, y, (const FcChar8 *)s, len);
    XftColorFree(dpy, DefaultVisual(dpy, screen), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, screen), &col);
}

static Pixmap g_hq_buf = 0, g_popup_buf = 0, g_strip_buf = 0;
static GC g_hq_buf_gc = 0, g_popup_buf_gc = 0, g_strip_buf_gc = 0;
static int g_hq_buf_h = 0, g_hq_buf_w = 0;
static int g_popup_buf_h = 0, g_popup_buf_w = 0;

/* RGB compose->present refactor (2026-08-12, "this seems like a good
 * time to do the tb refactor" - after db-hq's own conversion ran
 * confirmed-stable for real). Same pattern as khtpm_hq_render.c's own
 * g_frame_rgb: each of this app's THREE windows (strip/hq/popup) keeps
 * ONE persistent RGB buffer holding "what's actually on screen right
 * now" for that window, refreshed each present via one real XImage
 * capture + XPutImage (proven pixel-identical to XCopyArea in the
 * !.khtpm-rgb-refactor.md Phase 0 test, then confirmed again on
 * db-hq's own real code). present_rgb() is the one shared helper all
 * three call sites use instead of each doing its own bare XCopyArea -
 * composition (everything drawn into g_*_buf above) is untouched. */
static unsigned char *g_strip_rgb = NULL; static int g_strip_rgb_w = 0, g_strip_rgb_h = 0;
static unsigned char *g_hq_rgb = NULL;    static int g_hq_rgb_w = 0, g_hq_rgb_h = 0;
static unsigned char *g_popup_rgb = NULL; static int g_popup_rgb_w = 0, g_popup_rgb_h = 0;

static void present_rgb(Display *dpy, Pixmap buf, Window win, GC gc, int w, int h,
                         unsigned char **rgb_slot, int *rgb_w, int *rgb_h) {
    XSync(dpy, False);
    XImage *img = XGetImage(dpy, buf, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (!img) { XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)w, (unsigned)h, 0, 0); return; }
    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
    if (*rgb_w != w || *rgb_h != h) {
        free(*rgb_slot);
        *rgb_slot = malloc((size_t)w * h * 3);
        *rgb_w = w; *rgb_h = h;
    }
    if (*rgb_slot) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned long px = XGetPixel(img, x, y);
                size_t o = ((size_t)y * w + x) * 3;
                (*rgb_slot)[o]     = (unsigned char)((px >> 16) & 0xff);
                (*rgb_slot)[o + 1] = (unsigned char)((px >> 8) & 0xff);
                (*rgb_slot)[o + 2] = (unsigned char)(px & 0xff);
            }
        }
    }
    XDestroyImage(img);
}

/* One header cell's drawn string + pixel width, matching the old
 * header_cell_width() formula exactly (strlen*8+20, min 40) so drawn
 * pixels and hit-test rects can never drift apart.
 *
 * Real bug fix (2026-08-11, direct live report: "seems the top nav no
 * longer has numbering"): the .chtpm layout's <button label="..."> is
 * plain text (by design — the tag vocabulary has no nav-number
 * attribute), but the OLD hardcoded header_cell_width() always baked
 * "%d. " into the drawn string (cursor + index + label), and that visible
 * number is the SAME number sync_strip_claims()/ktb_jump_nav()/
 * max_claimed_nav() (all fixed earlier this session) use for digit-jump —
 * dropping it silently also breaks the visible meaning of "what number do
 * I type." nav_n (1-based, 0 = don't show a number, e.g. for non-header
 * rows) is now an explicit parameter, matching the header loop's own
 * running position counter below (which equals sync_strip_claims()'s own
 * NAV=i+1 assignment for root-level header cells). */
/* Real tab sprites (2026-08-11, direct request: "i want to put sprites
 * back"), ported VERBATIM from tp_taskbar.c's own tab_sprite()/
 * blit_tab_sprite() (read in full at ~line 915-1015 before writing this —
 * do not re-derive the format from memory/guesswork). Format: "<entity
 * dir>/sprite.csv" — a "# resolution=N" header line, then N*N "r,g,b,a"
 * rows, cached once per path, alpha-composited against the bar's solid
 * bg_pixel (RGBA-over-theme-background), drawn via XPutImage into whichever
 * offscreen double-buffer the caller passes (draw_header_win() and
 * draw_bottom() each have their own). Missing/unreadable sprite.csv =
 * text-only fallback, never a crash — matches legacy's own explicit
 * behavior exactly. Moved here (before format_cell()/draw_header_win(),
 * which now also use these) from its original spot right before
 * draw_bottom() — C requires the definition before use in the same
 * translation unit and both windows need this now, not just the bottom
 * bar's tabs. */
#define TAB_SPRITE_PX 24
typedef struct {
    char path[SP_PATH_BUF];
    unsigned char *rgba; /* res * res * 4 */
    int res;
} TabSprite;
static TabSprite g_sprite_cache[KTB_MAX_TABS];

static TabSprite *tab_sprite(const char *path) {
    if (!path || !path[0]) return NULL;
    for (int i = 0; i < KTB_MAX_TABS; i++) {
        if (g_sprite_cache[i].rgba && strcmp(g_sprite_cache[i].path, path) == 0) return &g_sprite_cache[i];
    }
    char csv_path[SP_PATH_BUF];
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
    for (int i = 0; i < KTB_MAX_TABS; i++) {
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

static int format_cell(LayDoc *doc, int idx, int nav_n, char *out, size_t outsz) {
    char label[LAY_LABEL_LEN];
    lay_get_label(doc, idx, sp_get_var, &g_st, label, sizeof(label));
    if (nav_n > 0)
        snprintf(out, outsz, "%s %d. %s", lay_cursor_prefix(doc, idx), nav_n, label);
    else
        snprintf(out, outsz, "%s %s", lay_cursor_prefix(doc, idx), label);
    int w = (int)strlen(out) * 8 + 20;
    /* Real gap fix (2026-08-11, "user should have a sprite at least"):
     * matches tp_taskbar.c's own header-cell sprite width accounting
     * exactly (`if (sp) w += TAB_SPRITE_PX + 6;` at ~line 1617) — the
     * fixed hq_win window must be sized wide enough to include the
     * avatar sprite, or cells after it get clipped again (the earlier
     * cutoff regression, same root cause class). MUST substitute via
     * lay_get_sprite() first, not check the raw attribute string — a raw
     * "${avatar_dir}" literal is never empty even when no avatar exists,
     * which would inflate width for every cell with a sprite= attribute
     * regardless of whether one actually resolves. */
    char sprite_path[LAY_SPRITE_LEN];
    lay_get_sprite(doc, idx, sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
    if (sprite_path[0]) w += TAB_SPRITE_PX + 6;
    if (w < 40) w = 40;
    return w;
}

/* Dry-run width sum (root-level buttons only, cli_io excluded) — used both
 * for initial hq_win sizing and as the header_w basis inside the real draw
 * function, mirroring the old compute_header_width()/header_cell_width()
 * split. Numbers cells 1..N in document order, matching header_win's own
 * loop and sync_strip_claims()'s NAV=i+1 claim numbering. */
static int header_total_width(LayDoc *doc) {
    int w = STRIP_NAV_BOX_W;
    int n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "button") != 0) continue;
        n++;
        char disp[96];
        w += format_cell(doc, i, n, disp, sizeof(disp)) + 6 /* STRIP_PAD */;
    }
    return w;
}

static void draw_header_win(Display *dpy, Window win, GC gc, LayDoc *doc, SpState *st) {
    int h = KTB_BAR_H;
    int header_w = STRIP_NAV_BOX_W;
    g_header_hit_n = 0;

    /* First pass computed inline below while drawing, same as the old
     * draw_header_win() did — one pass produces both pixels and hit rects. */
    Window win_real = win;
    int w_guess = header_total_width(doc);
    if (w_guess < 1) w_guess = 1;
    if (!g_hq_buf || g_hq_buf_h < h || g_hq_buf_w < w_guess) {
        if (g_hq_buf) { XFreePixmap(dpy, g_hq_buf); XFreeGC(dpy, g_hq_buf_gc); }
        if (g_hq_xft) { XftDrawDestroy(g_hq_xft); g_hq_xft = NULL; }
        g_hq_buf = XCreatePixmap(dpy, win, w_guess, h, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_hq_buf_gc = XCreateGC(dpy, g_hq_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_hq_buf_gc);
        g_hq_buf_h = h;
        g_hq_buf_w = w_guess;
        g_hq_xft = XftDrawCreate(dpy, g_hq_buf, DefaultVisual(dpy, DefaultScreen(dpy)), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, DefaultScreen(dpy)));
    }
    unsigned long fg, bg_pixel;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    { XGCValues gv; XGetGCValues(dpy, gc, GCBackground, &gv); bg_pixel = gv.background; }
    GC bgc = g_hq_buf_gc;
    XSetForeground(dpy, bgc, bg_pixel);
    XFillRectangle(dpy, g_hq_buf, bgc, 0, 0, w_guess, h);
    XSetForeground(dpy, bgc, fg);
    XDrawRectangle(dpy, g_hq_buf, bgc, 0, 0, w_guess - 1, h - 1);

    /* Real X11 focus indicator, direct request (2026-08-11): "does it have
     * focus? right click doesn't say... lets add a '^' in nav section if
     * it has focus, for sanity". Shown ALWAYS (not gated on nav_armed) so
     * this box is a persistent sanity check, not just visible while armed
     * — this is the ground truth for whether KeyPress events can reach
     * this process at all (g_has_real_focus is set only from real
     * FocusIn/FocusOut events, not merely "we called XSetInputFocus"). */
    {
        char arm_lab[72];
        const char *focus_mark = g_has_real_focus ? "^" : " ";
        if (st->nav_armed || st->digit_buf[0]) {
            const char *arm = st->digit_buf[0] ? st->digit_buf : "NAV";
            snprintf(arm_lab, sizeof(arm_lab), "%s[%s]", focus_mark, arm);
        } else {
            snprintf(arm_lab, sizeof(arm_lab), "%s", focus_mark);
        }
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_hq_xft, fg, 4, KTB_BAR_H / 2 + 4, arm_lab, (int)strlen(arm_lab));
    }
    XDrawLine(dpy, g_hq_buf, bgc, STRIP_NAV_BOX_W, 0, STRIP_NAV_BOX_W, KTB_BAR_H);

    int first = 1;
    int cell_n = 0;
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue;
        if (strcmp(doc->elements[i].type, "button") != 0) continue;
        cell_n++;
        if (!first) XDrawLine(dpy, g_hq_buf, bgc, header_w, 0, header_w, KTB_BAR_H);
        first = 0;
        char disp[96];
        int cw = format_cell(doc, i, cell_n, disp, sizeof(disp));
        /* Real gap fix (2026-08-11, "user should have a sprite at
         * least"): matches tp_taskbar.c's own header-cell sprite blit
         * exactly (sprite at x+4, text shifts to x+TAB_SPRITE_PX+10 —
         * ~line 1622-1625) — the ONLY place this cell type differs from
         * a bottom-bar tab's sprite positioning (x+2/x+8+PX there). */
        char sprite_path[LAY_SPRITE_LEN];
        lay_get_sprite(doc, i, sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
        TabSprite *sp = sprite_path[0] ? tab_sprite(sprite_path) : NULL;
        int text_x = header_w + 6;
        if (sp) {
            blit_tab_sprite(dpy, g_hq_buf, bgc, sp, header_w + 4, (KTB_BAR_H - TAB_SPRITE_PX) / 2, TAB_SPRITE_PX, bg_pixel);
            text_x = header_w + TAB_SPRITE_PX + 10;
        }
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_hq_xft, fg, text_x, KTB_BAR_H / 2 + 4, disp, (int)strlen(disp));
        if (g_header_hit_n < SP_MAX_HIT) {
            SpHitRect *r = &g_header_hits[g_header_hit_n++];
            r->x0 = header_w; r->x1 = header_w + cw; r->y0 = 0; r->y1 = KTB_BAR_H; r->elidx = i;
        }
        header_w += cw + 6;
    }
    g_header_row_w = header_w;

    present_rgb(dpy, g_hq_buf, win_real, g_hq_buf_gc, w_guess, h, &g_hq_rgb, &g_hq_rgb_w, &g_hq_rgb_h);
    XFlush(dpy);
}

/* Popup window: submenu rows (descendants of active_index) OR the single
 * cli_io field row when active_index points at the cli_io element. Returns
 * 1 if content was drawn (caller should map the window), 0 otherwise. */
static int draw_popup_win(Display *dpy, Window win, GC gc, LayDoc *doc, SpState *st,
                           int hq_win_x, int hq_win_y) {
    g_popup_hit_n = 0;
    if (doc->active_index < 0) return 0;

    int is_cliio = (strcmp(doc->elements[doc->active_index].type, "cli_io") == 0);
    int rows = 0;
    int row_elidx[KTB_LIVEDESK_DYN_MAX];
    if (is_cliio) {
        if (!st->cliio_active) return 0;
        rows = 1;
    } else {
        for (int i = 0; i < doc->element_count && rows < KTB_LIVEDESK_DYN_MAX; i++) {
            if (i == doc->active_index) continue;
            if (!lay_is_navigable(doc, i)) continue;
            if (!lay_is_descendant(doc, i, doc->active_index)) continue;
            row_elidx[rows++] = i;
        }
        if (rows == 0) return 0;
    }

    int h = KTB_BAR_H * rows;
    int w = 40, anchor_x0 = 0;
    for (int i = 0; i < g_header_hit_n; i++) {
        if (g_header_hits[i].elidx == doc->active_index) { anchor_x0 = g_header_hits[i].x0; break; }
    }
    if (is_cliio) {
        /* Real bug fix (2026-08-11, direct live report: "that toolbar u
         * just open isn't placed under its tab. it takes up entire
         * header bar width and is confusing"). This used to force
         * w=g_header_row_w (the FULL header bar's total width) and
         * anchor_x0=0 (leftmost, under HQ) for the cli-io modal
         * specifically — regardless of which cell actually opened it.
         * Confirmed via direct read of legacy's real cliio_open()/
         * cliio_open_save_as() (tp_taskbar.c ~line 1450/2731): this is
         * NOT how legacy positions it at all — legacy's own comment is
         * explicit: "SEPARATE standalone window for the edit -
         * deliberately NOT anchored to the toolbar (direct instruction):
         * centered on screen instead." Legacy uses a fixed w=300,
         * horizontally centered on the real screen width, fixed y=140 —
         * completely independent of the header bar's own position/width.
         * Ported exactly; win_x/win_y are set directly below for this
         * branch instead of the generic hq_win_x+anchor_x0 formula. */
        w = 300;
    } else {
        for (int r = 0; r < rows; r++) {
            char lab[192];
            char label[LAY_LABEL_LEN];
            lay_get_label(doc, row_elidx[r], sp_get_var, &g_st, label, sizeof(label));
            /* Real gap fix (2026-08-11, direct report: "the submenu is
             * missing numbers. it should be auto/dynamically numbered...
             * how chtpm worked") — row numbers were never drawn at all,
             * matching neither legacy's popup_digit() rows nor real
             * CHTPM's own numbered menu rendering. r+1 (1-based) matches
             * ktb_hq_digit()'s own `accum - 1` convention exactly, so the
             * number shown is the number to type. */
            snprintf(lab, sizeof(lab), "%s %d. %s", lay_cursor_prefix(doc, row_elidx[r]), r + 1, label);
            int cw = (int)strlen(lab) * 8 + 24;
            if (cw > w) w = cw;
        }
    }
    int win_x, win_y;
    if (is_cliio) {
        /* Matches legacy's cliio_open()/cliio_open_save_as() exactly:
         * centered horizontally on the real screen, fixed y=140. */
        int sw = DisplayWidth(dpy, DefaultScreen(dpy));
        win_x = (sw - w) / 2;
        win_y = 140;
    } else {
        win_x = hq_win_x + anchor_x0;
        win_y = hq_win_y + KTB_BAR_H;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    Window win_real = win;
    if (!g_popup_buf || g_popup_buf_h < h || g_popup_buf_w < w) {
        if (g_popup_buf) { XFreePixmap(dpy, g_popup_buf); XFreeGC(dpy, g_popup_buf_gc); }
        if (g_popup_xft) { XftDrawDestroy(g_popup_xft); g_popup_xft = NULL; }
        g_popup_buf = XCreatePixmap(dpy, win, w, h, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_popup_buf_gc = XCreateGC(dpy, g_popup_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_popup_buf_gc);
        g_popup_buf_h = h;
        g_popup_buf_w = w;
        g_popup_xft = XftDrawCreate(dpy, g_popup_buf, DefaultVisual(dpy, DefaultScreen(dpy)), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, DefaultScreen(dpy)));
    }
    unsigned long fg, bg_pixel;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    { XGCValues gv; XGetGCValues(dpy, gc, GCBackground, &gv); bg_pixel = gv.background; }
    GC bgc = g_popup_buf_gc;
    XSetForeground(dpy, bgc, bg_pixel);
    XFillRectangle(dpy, g_popup_buf, bgc, 0, 0, w, h);
    XSetForeground(dpy, bgc, fg);
    XDrawRectangle(dpy, g_popup_buf, bgc, 0, 0, w - 1, h - 1);

    if (is_cliio) {
        const char *pref = st->cliio_typing ? "[^]" : "[>]";
        char row0[320];
        if (st->cliio_typing)
            snprintf(row0, sizeof(row0), "%s %s: [%s_]  (Enter=save, Esc=cancel)", pref, st->cliio_label, st->cliio_buffer);
        else
            snprintf(row0, sizeof(row0), "%s %s: [%s]  (Enter=edit, Esc=cancel)", pref, st->cliio_label, st->cliio_buffer);
        strip_draw_utf8(dpy, DefaultScreen(dpy), g_popup_xft, fg, 8, KTB_BAR_H / 2 + 4, row0, (int)strlen(row0));
        if (g_popup_hit_n < SP_MAX_HIT) {
            SpHitRect *r = &g_popup_hits[g_popup_hit_n++];
            r->x0 = 0; r->x1 = w; r->y0 = 0; r->y1 = KTB_BAR_H; r->elidx = doc->active_index;
        }
    } else {
        for (int r = 0; r < rows; r++) {
            int row_y = KTB_BAR_H * r;
            if (r > 0) XDrawLine(dpy, g_popup_buf, bgc, 0, row_y, w, row_y);
            char label[LAY_LABEL_LEN];
            lay_get_label(doc, row_elidx[r], sp_get_var, &g_st, label, sizeof(label));
            char lab[192];
            snprintf(lab, sizeof(lab), "%s %d. %s", lay_cursor_prefix(doc, row_elidx[r]), r + 1, label);
            strip_draw_utf8(dpy, DefaultScreen(dpy), g_popup_xft, fg, 12, row_y + KTB_BAR_H / 2 + 4, lab, (int)strlen(lab));
            if (g_popup_hit_n < SP_MAX_HIT) {
                SpHitRect *hr = &g_popup_hits[g_popup_hit_n++];
                hr->x0 = 0; hr->x1 = w; hr->y0 = row_y; hr->y1 = row_y + KTB_BAR_H; hr->elidx = row_elidx[r];
            }
        }
    }

    XMoveResizeWindow(dpy, win_real, win_x, win_y, w, h);
    present_rgb(dpy, g_popup_buf, win_real, g_popup_buf_gc, w, h, &g_popup_rgb, &g_popup_rgb_w, &g_popup_rgb_h);
    XFlush(dpy);
    return 1;
}

/* Bottom bar: walks khtpm_strip_bottom.chtpm's two <row> groups (tabs,
 * shortcuts) horizontally, one row of pixels each — replaces the old
 * hardcoded draw_strip() tab loop with the SAME horizontal layout shape,
 * now driven by whatever ${strip_tabs}/${strip_shortcuts} actually
 * contain rather than a KtbTab[] array read directly. */
static void draw_bottom(Display *dpy, Window win, GC gc, int sw, unsigned long bg_pixel, LayDoc *doc) {
    g_bottom_hit_n = 0;
    Window win_real = win;
    if (!g_strip_buf) {
        g_strip_buf = XCreatePixmap(dpy, win, sw, KTB_BAR_H, DefaultDepth(dpy, DefaultScreen(dpy)));
        g_strip_buf_gc = XCreateGC(dpy, g_strip_buf, 0, NULL);
        XCopyGC(dpy, gc, GCForeground | GCBackground | GCFont, g_strip_buf_gc);
        g_strip_xft = XftDrawCreate(dpy, g_strip_buf, DefaultVisual(dpy, DefaultScreen(dpy)), g_strip_cmap ? g_strip_cmap : DefaultColormap(dpy, DefaultScreen(dpy)));
    }
    unsigned long fg;
    { XGCValues gv; XGetGCValues(dpy, gc, GCForeground, &gv); fg = gv.foreground; }
    GC bgc = g_strip_buf_gc;
    XSetForeground(dpy, bgc, bg_pixel);
    XFillRectangle(dpy, g_strip_buf, bgc, 0, 0, sw, KTB_BAR_H);
    XSetForeground(dpy, bgc, fg);
    win = g_strip_buf;

    XDrawLine(dpy, win, bgc, 0, 0, sw, 0);

    int x = 8;
    for (int i = 0; i < doc->element_count; i++) {
        if (doc->elements[i].parent_index != -1) continue; /* root <row> groups */
        if (strcmp(doc->elements[i].type, "row") != 0) continue;
        for (int c = 0; c < doc->elements[i].num_children; c++) {
            int ci = doc->elements[i].children[c];
            if (strcmp(doc->elements[ci].type, "button") != 0) continue;
            char label[LAY_LABEL_LEN];
            lay_get_label(doc, ci, sp_get_var, &g_st, label, sizeof(label));
            char lab[192];
            snprintf(lab, sizeof(lab), "%s %s", lay_cursor_prefix(doc, ci), label);
            /* Real sprite fix (2026-08-11, "i want to put sprites back"):
             * matches tp_taskbar.c's own draw_bar() exactly — sprite at
             * x0+2 (2px left margin), text shifts right by TAB_SPRITE_PX
             * only when a real sprite was actually loaded (missing/
             * unreadable sprite.csv = plain text at the original x0+8,
             * no reserved gap). Width must account for the sprite too,
             * or hit-test rects (and the next tab's start x) would drift
             * from what's actually drawn. */
            char sprite_path[LAY_SPRITE_LEN];
            lay_get_sprite(doc, ci, sp_get_var, &g_st, sprite_path, sizeof(sprite_path));
            TabSprite *sp = tab_sprite(sprite_path);
            int w = (int)strlen(lab) * 8 + 16 + (sp ? TAB_SPRITE_PX : 0);
            if (x + w >= sw - 8) break;
            XDrawLine(dpy, win, bgc, x, 0, x, KTB_BAR_H);
            int text_x = x + 8;
            if (sp) {
                int sy = (KTB_BAR_H - TAB_SPRITE_PX) / 2;
                blit_tab_sprite(dpy, win, bgc, sp, x + 2, sy, TAB_SPRITE_PX, bg_pixel);
                text_x = x + 8 + TAB_SPRITE_PX;
            }
            strip_draw_utf8(dpy, DefaultScreen(dpy), g_strip_xft, fg, text_x, KTB_BAR_H / 2 + 4, lab, (int)strlen(lab));
            if (g_bottom_hit_n < SP_MAX_HIT) {
                SpHitRect *r = &g_bottom_hits[g_bottom_hit_n++];
                r->x0 = x; r->x1 = x + w; r->y0 = 0; r->y1 = KTB_BAR_H; r->elidx = ci;
            }
            x += w;
        }
    }

    present_rgb(dpy, g_strip_buf, win_real, g_strip_buf_gc, sw, KTB_BAR_H, &g_strip_rgb, &g_strip_rgb_w, &g_strip_rgb_h);
    XFlush(dpy);
}

static int hit_test(SpHitRect *hits, int n, int x, int y) {
    for (int i = 0; i < n; i++) {
        if (x >= hits[i].x0 && x < hits[i].x1 && y >= hits[i].y0 && y < hits[i].y1)
            return hits[i].elidx;
    }
    return -1;
}

/* Layout file paths, sibling to this ops/ dir's parent (*.livedesk-taskbar/). */
static void layout_path(char *out, size_t n, const char *filename) {
    char rel[256];
    snprintf(rel, sizeof(rel), "*.monads/*.livedesk-taskbar/%s", filename);
    path_join2(out, n, g_house_root, rel);
}

/* Reload both docs + the one cli_io special case: if the manager says
 * cliio_active but our local header_doc doesn't yet have it as the active
 * scope, activate it locally; if cliio just closed, back out of it. See
 * khtpm_strip_layout.c's lay_is_navigable() header comment for why this is
 * safe (cli_io is only ever navigable while it IS active_index). */
static void sync_cliio_scope(LayDoc *header_doc, SpState *st) {
    int cliio_idx = -1;
    for (int i = 0; i < header_doc->element_count; i++) {
        if (strcmp(header_doc->elements[i].type, "cli_io") == 0) { cliio_idx = i; break; }
    }
    if (cliio_idx < 0) return;
    if (st->cliio_active && header_doc->active_index != cliio_idx) {
        lay_activate(header_doc, cliio_idx);
    } else if (!st->cliio_active && header_doc->active_index == cliio_idx) {
        lay_back(header_doc);
    }
}

/* Real gap fix (2026-08-11, live-verified via the agent relay: typing a
 * row digit inside an open HQ/player submenu did nothing at all — no
 * visible cursor movement, and Enter right after was silently swallowed).
 * Root cause: the MANAGER already tracks the open submenu's own local row
 * cursor correctly (s->hq_focus, moved by ktb_hq_digit()/
 * ktb_hq_focus_delta(), published as strip_state.txt's own "hq_focus" KEY
 * row — this was never missing manager-side) but the PARSER never read it
 * back into header_doc->focus_index at all. So typing a digit moved the
 * manager's hq_focus with zero visible effect here (lay_cursor_prefix()
 * only ever reads header_doc->focus_index, never st->hq_focus), and Enter
 * (dispatch_key_code()'s KSC_ENTER branch) checks
 * lay_is_navigable(header_doc, header_doc->focus_index) — which was still
 * -1 from before the popup opened (per unified_apply()'s own contract),
 * so nothing ever dispatched. Fixed the same way sync_focus_to_digit_buf()
 * closes the equivalent top-level gap: read the manager's own cursor
 * position back, map it onto the SAME row order draw_popup_win() already
 * builds (navigable descendants of active_index, in document order), and
 * write it into header_doc->focus_index — the single derived-state field
 * everything else (cursor rendering, Enter dispatch) already reads. Only
 * arms while a non-cli_io submenu is genuinely open; arrow-key navigation
 * (lay_focus_delta()) stays untouched and still works exactly as before —
 * this only adds the missing digit-driven path. */
static char g_popup_focus_last_onclick[LAY_ONCLICK_LEN] = ""; /* see grace-period note below */

static void sync_popup_focus(LayDoc *header_doc, const SpState *st) {
    if (header_doc->active_index < 0) { g_popup_focus_last_onclick[0] = '\0'; return; }
    if (strcmp(header_doc->elements[header_doc->active_index].type, "cli_io") == 0) return;
    /* Real bug fix (2026-08-11, found live during an actual save/load
     * harness test): dispatch_onclick()'s HQITEM: branch no longer
     * lay_back()s unconditionally after every row (see that function's
     * own comment — "load" replaces the popup content in place instead
     * of closing, and hardcoding "always close" broke it). This is the
     * other half: the manager's real hq_open (0 = nothing open) is the
     * authoritative signal for when a LOCAL scope should actually close
     * — e.g. after "save"/"new-desk" genuinely close it.
     *
     * GRACE PERIOD, real and necessary (found while implementing this,
     * not hypothetical): dispatch_onclick()'s ACTIVATE branch opens the
     * local scope OPTIMISTICALLY (lay_activate() runs immediately,
     * before any manager round-trip) — the manager's own hq_open only
     * catches up after it polls strip_history.txt on ITS next tick and
     * republishes. Trusting hq_open==0 unconditionally would slam a
     * freshly-opened menu shut on its very first sync tick, before the
     * manager's real confirmation ever arrives, since nothing re-opens
     * it afterward. Only trust hq_open==0 once the SAME active scope has
     * been observed stable across at least one prior tick.
     *
     * REAL BUG FOUND IMPLEMENTING THIS (not hypothetical either): the
     * first version compared header_doc->active_index (a raw array
     * index) across ticks — but lay_load()'s own documented contract is
     * that indices are NOT stable across reloads when element COUNT
     * changes (e.g. ${strip_hq_items} growing from a 4-row file menu to
     * a 7-row session picker shifts every later cell's array position).
     * Live-confirmed: this caused an infinite postponement — the index
     * legitimately changed every tick due to reload-driven shifting, so
     * "stable" was never observed and the close never fired at all.
     * Fixed to compare the ACTIVE ELEMENT'S OWN onClick STRING instead
     * (e.g. "ACTIVATE:3") — the same identity lay_reload_preserving_
     * scope() itself already uses to survive reloads, genuinely stable
     * regardless of array-position churn. */
    const char *cur_onclick = header_doc->elements[header_doc->active_index].onClick;
    if (!st->hq_open) {
        if (strcmp(cur_onclick, g_popup_focus_last_onclick) == 0) {
            lay_back(header_doc);
            g_popup_focus_last_onclick[0] = '\0';
            return;
        }
        /* freshly activated this tick — give the manager one more cycle
         * before trusting hq_open==0. */
    }
    snprintf(g_popup_focus_last_onclick, sizeof(g_popup_focus_last_onclick), "%s", cur_onclick);
    int n = 0, target = -1;
    for (int i = 0; i < header_doc->element_count; i++) {
        if (i == header_doc->active_index) continue;
        if (!lay_is_navigable(header_doc, i)) continue;
        if (!lay_is_descendant(header_doc, i, header_doc->active_index)) continue;
        if (n == st->hq_focus) { target = i; break; }
        n++;
    }
    if (target >= 0) header_doc->focus_index = target;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: khtpm_strip_parser <house_root>\n");
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    frame_history_init();
    int hq_win_x = 0, hq_win_y = 40;
    load_strip_offset(&hq_win_x, &hq_win_y);

    signal(SIGTERM, on_sigterm);
    signal(SIGINT, on_sigterm);

    launch_manager();
    wait_for_manager_first_publish();

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "strip_parser: no display\n");
        cleanup_manager();
        return 1;
    }
    int sw = DisplayWidth(dpy, DefaultScreen(dpy));
    int sh = DisplayHeight(dpy, DefaultScreen(dpy));
    g_strip_cmap = DefaultColormap(dpy, DefaultScreen(dpy));

    load_state(&g_st);

    char header_path[SP_PATH_BUF], bottom_path[SP_PATH_BUF];
    layout_path(header_path, sizeof(header_path), "khtpm_strip_header.chtpm");
    layout_path(bottom_path, sizeof(bottom_path), "khtpm_strip_bottom.chtpm");

    static LayDoc header_doc, bottom_doc;
    if (!lay_load(&header_doc, header_path, sp_get_var, &g_st))
        fprintf(stderr, "strip_parser: failed to load %s\n", header_path);
    if (!lay_load(&bottom_doc, bottom_path, sp_get_var, &g_st))
        fprintf(stderr, "strip_parser: failed to load %s\n", bottom_path);
    /* Real bug fix (2026-08-11, direct live report — the visible "[>] 13.
     * self" turned out to be the bottom bar's own legitimate first tab,
     * correctly nav-numbered 13 by the shared claims file continuing after
     * the 12 header cells, NOT a cli_io/off-by-one bug): header_doc and
     * bottom_doc are two SEPARATE LayDoc instances (per the one-file-per-
     * window decision), and lay_load() seeds EACH one's own focus_index to
     * its own first navigable element independently — so both windows
     * showed a live [>] cursor at once. Legacy's real design has exactly
     * ONE unified cursor across header+tabs (nav_focus_apply()/
     * nav_focus_step()), always starting on header cell 1 (HQ) — matches
     * the direct instruction "focus should go to 1 always like chtpm."
     * Minimal correct fix for now (full cross-window arrow-key unification
     * is a separate, larger task, not yet built — bottom bar stays mouse-
     * click-only per this pass's own already-documented limitation):
     * bottom_doc starts with NO visible focus at all; only header_doc's
     * cursor is live until a tab is actually clicked. */
    bottom_doc.focus_index = -1;
    /* Real bug fix, part 2 (the first attempt's -1 got silently undone):
     * lay_reload_preserving_scope() treats ANY negative focus_index as
     * "invalid, needs reseeding to the first navigable element" (correct
     * for the header, where -1 genuinely means "lost track, recover") —
     * but this bottom bar's -1 means something different: "intentionally
     * unfocused until the user clicks a tab." Every ~300ms redraw tick
     * calls lay_reload_preserving_scope() on bottom_doc too, silently
     * re-seeding it back to tab 1 each time. SUPERSEDED by the unified-
     * cursor rework above (g_nav_focus/unified_apply()) — that flag-based
     * approach was itself the wrong shape (see this file's own "focus is
     * for both" comment near g_nav_focus's declaration), replaced
     * entirely rather than kept alongside it. */
    unified_apply(&header_doc, &bottom_doc);
    sync_cliio_scope(&header_doc, &g_st);

    unsigned long bg = parse_color(dpy, g_st.theme_bg, BlackPixel(dpy, DefaultScreen(dpy)));
    unsigned long fg = parse_color(dpy, g_st.theme_fg, WhitePixel(dpy, DefaultScreen(dpy)));

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                               0, sh - KTB_BAR_H, sw, KTB_BAR_H, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    taskbar_set_wm_class(dpy, win);
    XMapRaised(dpy, win);
    set_window_opacity(dpy, win, load_theme_opacity());
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, fg);
    XSetBackground(dpy, gc, bg);

    int hq_real_w = header_total_width(&header_doc);
    if (hq_real_w > sw - hq_win_x) hq_real_w = sw - hq_win_x;
    XSetWindowAttributes hq_swa;
    hq_swa.override_redirect = True;
    hq_swa.background_pixel = bg;
    hq_swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask;
    Window hq_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  hq_win_x, hq_win_y, hq_real_w, KTB_BAR_H, 0,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &hq_swa);
    taskbar_set_wm_class(dpy, hq_win);
    XMapRaised(dpy, hq_win);
    set_window_opacity(dpy, hq_win, load_theme_opacity());

    XSetWindowAttributes popup_swa;
    popup_swa.override_redirect = True;
    popup_swa.background_pixel = bg;
    popup_swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask | FocusChangeMask;
    Window popup_win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                     hq_win_x, hq_win_y + KTB_BAR_H, 200, KTB_BAR_H, 0,
                                     CopyFromParent, InputOutput, CopyFromParent,
                                     CWOverrideRedirect | CWBackPixel | CWEventMask, &popup_swa);
    taskbar_set_wm_class(dpy, popup_win);
    set_window_opacity(dpy, popup_win, load_theme_opacity());
    int popup_mapped = 0;

    draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
    draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
    if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
        XMapRaised(dpy, popup_win);
        popup_mapped = 1;
        draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
    }

    taskbar_soft_focus(dpy, win);

    /* KISS opacity-on-reset fix (2026-08-11, direct question: "did u ever
     * figure out how legacy would have transparency on bottom bar (but
     * only after reset)?" — see opacity-bug-aug9.txt and
     * khtpm-refactor-plan.md's "Opacity-on-reset quirk" entry, both
     * previously inconclusive/deferred, not actually fixed). Leading
     * theory there: a compositor-timing quirk where Mutter/Xwayland
     * doesn't reliably honor _NET_WM_WINDOW_OPACITY set at map-time on the
     * FIRST paint of an override-redirect window, but does honor it once
     * the property is re-applied after the window has been visible for at
     * least one real frame — which is incidentally exactly what "reset"
     * (a full process relaunch) does. Cheapest possible fix matching that
     * theory, never actually tried before now: just re-set the SAME
     * opacity property again here, after the windows are already
     * mapped/painted (first draw_header_win()/draw_bottom()/
     * draw_popup_win() calls above already ran), instead of only once at
     * XCreateWindow time. If the theory is right, this alone makes a
     * fresh launch behave like a post-reset one, with no reset needed. */
    XFlush(dpy);
    usleep(200000);
    double relaunch_opacity = load_theme_opacity();
    set_window_opacity(dpy, win, relaunch_opacity);
    set_window_opacity(dpy, hq_win, relaunch_opacity);
    set_window_opacity(dpy, popup_win, relaunch_opacity);
    XFlush(dpy);

    while (g_running) {
        reap_manager_nonblocking();

        if (frame_changed_dirty()) {
            load_state(&g_st);
            lay_reload_preserving_scope(&header_doc, header_path, sp_get_var, &g_st);
            lay_reload_preserving_scope(&bottom_doc, bottom_path, sp_get_var, &g_st);
            /* g_nav_focus (the unified index) is the sole source of truth
             * for top-level (no submenu open) focus now — override
             * whatever lay_reload_preserving_scope()'s own internal
             * onClick-matching seeded independently on each doc, matching
             * tp_taskbar.c's own nav_focus_apply() being "the single place
             * that ever writes" the two derived variables. Skipped while a
             * header submenu is open (active_index != -1), since that
             * state is governed by lay_focus_delta()'s own LOCAL logic,
             * not the unified index. */
            if (header_doc.active_index == -1) unified_apply(&header_doc, &bottom_doc);
            sync_cliio_scope(&header_doc, &g_st);
            sync_popup_focus(&header_doc, &g_st);
            sync_focus_to_digit_buf(&header_doc, &bottom_doc, &g_st);

            append_frame_history(&header_doc, &bottom_doc, g_nav_focus);

            bg = parse_color(dpy, g_st.theme_bg, bg);
            fg = parse_color(dpy, g_st.theme_fg, fg);
            XSetWindowBackground(dpy, win, bg);
            XSetForeground(dpy, gc, fg);
            /* REAL FIX 2026-08-13: XSetBackground(dpy, gc, bg) was only
             * ever called once at startup (line ~1534), never again
             * here. draw_bottom()/draw_header_win()'s own sprite alpha-
             * blending (blit_tab_sprite()) reads the GC's CACHED
             * background via XGetGCValues(..., GCBackground, ...), not
             * the window's background_pixel attribute - so after any
             * live theme change, XSetWindowBackground below correctly
             * updates the window's own bg, but sprites kept blending
             * against the ORIGINAL startup color forever, producing a
             * visible top/bottom-bar (or pre/post-theme-change) color
             * mismatch once any live-reload happened after startup.
             * Direct user report: "colors have not been going back to
             * usual there is a discrepancy of discoloration between top
             * and bottom bar now." */
            XSetBackground(dpy, gc, bg);
            draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
            XSetWindowBackground(dpy, hq_win, bg);
            draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
            XSetWindowBackground(dpy, popup_win, bg);
            if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
                if (!popup_mapped) {
                    XMapRaised(dpy, popup_win);
                    popup_mapped = 1;
                    draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                }
            } else if (popup_mapped) {
                XUnmapWindow(dpy, popup_win);
                popup_mapped = 0;
            }
        }

        /* Agent relay poll — every loop tick, independent of
         * frame_changed_dirty() (an agent can inject input even when the
         * manager hasn't signaled anything new). See dispatch_key_code()'s
         * own header comment for the full contract. Same explicit-redraw
         * necessity as the real KeyPress handler below: a relay-driven
         * LOCAL state change (lay_back()/dispatch_onclick()'s ACTIVATE
         * branch) doesn't touch strip_frame_changed.txt either. */
        if (poll_agent_relay(g_house_root, &header_doc, &bottom_doc, &g_st) > 0) {
            draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
            draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
            if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
                if (!popup_mapped) {
                    XMapRaised(dpy, popup_win);
                    popup_mapped = 1;
                    draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                }
            } else if (popup_mapped) {
                XUnmapWindow(dpy, popup_win);
                popup_mapped = 0;
            }
        }

        fd_set fds;
        FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ButtonPress && ev.xbutton.window == hq_win) {
                if (ev.xbutton.button == 3) {
                    /* Matches tp_taskbar.c's own unconditional "nav_focus = 0"
                     * on EITHER window's right-click (~lines 3694/3731,
                     * direct comment: "header gets priority [>] focus") —
                     * right-click always arms the UNIFIED cursor at
                     * position 0 (header cell 1), regardless of which
                     * window was clicked; the whole point of unifying
                     * header+bottom into one index is that arrow keys can
                     * then walk from there into the OTHER window too. */
                    if (!g_st.cliio_active) { send_code(KSC_NAV_ARM); g_nav_focus = 0; unified_apply(&header_doc, &bottom_doc); taskbar_soft_focus(dpy, hq_win); }
                } else {
                    int elidx = hit_test(g_header_hits, g_header_hit_n, ev.xbutton.x, ev.xbutton.y);
                    if (elidx >= 0) {
                        header_doc.focus_index = elidx;
                        g_nav_focus = root_nav_pos_of(&header_doc, elidx);
                        dispatch_onclick(&header_doc, elidx);
                        taskbar_soft_focus(dpy, hq_win);
                    }
                }
            } else if (ev.type == ButtonPress && ev.xbutton.window == popup_win) {
                if (ev.xbutton.button == 3) {
                    if (!g_st.cliio_active) { send_code(KSC_NAV_ARM); g_nav_focus = 0; unified_apply(&header_doc, &bottom_doc); taskbar_soft_focus(dpy, popup_win); }
                } else {
                    int elidx = hit_test(g_popup_hits, g_popup_hit_n, ev.xbutton.x, ev.xbutton.y);
                    if (elidx >= 0) {
                        if (strcmp(header_doc.elements[elidx].type, "cli_io") == 0) {
                            send_code(KSC_ENTER); /* start/submit typing — manager-owned state */
                        } else {
                            header_doc.focus_index = elidx;
                            dispatch_onclick(&header_doc, elidx);
                        }
                        taskbar_soft_focus(dpy, popup_win);
                    }
                }
            } else if (ev.type == ButtonPress && ev.xbutton.window == win) {
                if (ev.xbutton.button == 3) {
                    send_code(KSC_NAV_ARM);
                    g_nav_focus = 0;
                    unified_apply(&header_doc, &bottom_doc);
                    taskbar_soft_focus(dpy, win);
                } else {
                    int elidx = hit_test(g_bottom_hits, g_bottom_hit_n, ev.xbutton.x, ev.xbutton.y);
                    if (elidx >= 0) {
                        bottom_doc.focus_index = elidx;
                        g_nav_focus = root_nav_count(&header_doc) + root_nav_pos_of(&bottom_doc, elidx);
                        dispatch_onclick(&bottom_doc, elidx);
                    }
                }
            } else if (ev.type == FocusIn) {
                g_has_real_focus = 1;
            } else if (ev.type == FocusOut) {
                g_has_real_focus = 0;
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Left || ks == XK_Up) {
                    if (header_doc.active_index != -1) lay_focus_delta(&header_doc, -1);
                    else unified_step(&header_doc, &bottom_doc, -1);
                } else if (ks == XK_Right || ks == XK_Down) {
                    if (header_doc.active_index != -1) lay_focus_delta(&header_doc, 1);
                    else unified_step(&header_doc, &bottom_doc, 1);
                } else if (ks == XK_Return) {
                    /* Unified focus fix: Enter must dispatch against
                     * whichever doc the unified cursor is CURRENTLY
                     * derived onto — header OR bottom, not header always
                     * (a tab reached by arrowing down from the header
                     * needs Enter to activate the TAB, matching legacy's
                     * own "empty digit_buf: activate focused button OR
                     * tab" Enter branch). Shared with poll_agent_relay()
                     * via dispatch_key_code() — see that function's own
                     * header comment for why sharing (not duplicating,
                     * unlike legacy) is the right call here. */
                    dispatch_key_code(&header_doc, &bottom_doc, &g_st, KSC_ENTER);
                } else if (ks == XK_Escape) {
                    dispatch_key_code(&header_doc, &bottom_doc, &g_st, KSC_ESCAPE);
                } else if (ks == XK_BackSpace) {
                    dispatch_key_code(&header_doc, &bottom_doc, &g_st, KSC_BACKSPACE);
                } else {
                    char buf[8] = {0};
                    if (XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL) > 0) {
                        unsigned char c = (unsigned char)buf[0];
                        if (c >= 0x20 && c < 0x7f) dispatch_key_code(&header_doc, &bottom_doc, &g_st, (int)c);
                    }
                }
                /* Real bug fix (2026-08-11, direct live report: "nav has
                 * focus but the selector isn't moving from arrow or
                 * digits yet"): arrow/Escape/local-ACTIVATE-Enter key
                 * handling above only ever changes header_doc's own
                 * in-memory focus_index/active_index — a purely LOCAL
                 * state change, no manager round-trip. But every redraw
                 * call in this whole file lived inside the
                 * frame_changed_dirty() block, gated entirely on the
                 * MANAGER's own strip_frame_changed.txt signal — a local
                 * key press never touched that file, so the screen never
                 * updated to reflect the cursor's real (correct, in
                 * memory) new position. Explicit redraw right here closes
                 * the gap, matching the same draw+map/unmap pattern the
                 * frame_changed_dirty() block already uses. draw_bottom()
                 * included too now that unified_step() can move focus
                 * into the bottom bar directly from a header-window key
                 * press. */
                draw_bottom(dpy, win, gc, sw, bg, &bottom_doc);
                draw_header_win(dpy, hq_win, gc, &header_doc, &g_st);
                if (draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y)) {
                    if (!popup_mapped) {
                        XMapRaised(dpy, popup_win);
                        popup_mapped = 1;
                        draw_popup_win(dpy, popup_win, gc, &header_doc, &g_st, hq_win_x, hq_win_y);
                    }
                } else if (popup_mapped) {
                    XUnmapWindow(dpy, popup_win);
                    popup_mapped = 0;
                }
            }
        }
    }

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XDestroyWindow(dpy, hq_win);
    XDestroyWindow(dpy, popup_win);
    XCloseDisplay(dpy);
    cleanup_manager();
    return 0;
}

#endif /* !_WIN32 */
