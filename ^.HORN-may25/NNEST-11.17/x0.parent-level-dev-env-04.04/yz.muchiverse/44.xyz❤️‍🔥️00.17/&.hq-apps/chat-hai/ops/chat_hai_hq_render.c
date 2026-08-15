/* chat_hai_hq_render.c — chat-hai IRC-like layout (left: sessions, right: messages+input),
 * built ON the house chtpm renderer standard (khtpm_hq_render.c).
 * Data: left sidebar shows session list, right panel shows transcript ledger
 * messages + composer input at bottom.
 *
 * Standalone binary, own window, own event loop, own tag-tree parser (reads
 * .chtpm vocabulary: window/sidebar/item/panel/title/text/button), styled
 * via khtpm_css_parser.c against a matching .css file.
 *
 * Usage: chat_hai_hq_render.+x <house_root> <chtpm_path>
 * The transcript ledger is the master-ledger file the chat loop appends to;
 * typing in the composer appends a "user:" line the next persona turn answers. */
#include "khtpm_css_parser.h"
#include "khtpm_taskbar_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <sys/select.h>

/* debug PNG dump (press 'p') - Xlib/Xft equivalent of the house's own
 * chtpm-rgb-render + dump_rgb_png.c convention (which reads back a GL
 * frame via glReadPixels for GLUT/GLX windows, since those are otherwise
 * unviewable to an agent). db-hq has no GL context - it composes into an
 * offscreen X Pixmap (see `buf` below) and blits with XCopyArea, so the
 * equivalent readback here is XGetImage on that same Pixmap, not
 * glReadPixels. Same vendored stb_image_write.h the house already uses
 * elsewhere (ops/lib/, copied from 014.wsr-pal+2/ops/lib/ - public domain,
 * not re-fetched). */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#define MAX_CHILDREN 64
#define MAX_ELEMS 512

typedef struct Elem {
    char tag[32];
    char id[64];
    char classes[CSS_MAX_CLASSES][32];
    int n_classes;
    char label[256];
    char onclick[64];
    int active;   /* tab active / sidebar item selected */
    int nav_index; /* wraith-alpha-standard index nav: 1-based sequential
                     * number assigned to every interactive element each
                     * redraw (see assign_nav_indices()); 0 = not navigable.
                     * Ported from 1.TPMOS_c_+rmmp.0103.0001/projects/
                     * wraith-alpha/ops/wraith_parser_alpha.c's own
                     * digit_accum/do_jump/display_num convention (direct
                     * instruction: "wraith alpha should be a huge
                     * inspiration for this"). */
    struct Elem *children[MAX_CHILDREN];
    int n_children;
    struct Elem *parent;
    /* computed layout, filled by layout_pass() */
    int x, y, w, h;
    CssStyle style;
} Elem;

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
/* REAL BUG FOUND 2026-08-15 (direct report: "clicking ON the message in
 * window crashed window"): g_n_elems is a bump-allocator index that
 * elem_new() NEVER rewinds. inject_sessions()/inject_panel_feed() (see
 * their own header comments) call elem_new() fresh every single
 * redraw() with no NULL-check before dereferencing the result - so
 * after ~MAX_ELEMS cumulative allocations across the session's whole
 * redraw history (not tied to any one click, just whichever redraw
 * happens to be the one that finally exhausts the pool), elem_new()
 * starts returning NULL and the very next `item->parent = ...` write
 * segfaults. g_n_elems_static is the fix: captured once, right after
 * parse_chtpm() in main(), as the count of REAL .chtpm-declared
 * elements; layout_pass() rewinds g_n_elems to this baseline every
 * frame before any dynamic injection runs, so the pool never grows
 * across frames - bounded and deterministic, not merely "big enough for
 * now." (Same underlying flaw existed in the file's original
 * inject_sidebar_items(), not something this session's edits
 * introduced - just newly triggered by feed items now living in the
 * panel instead of a shorter-lived sidebar-only list.) */
static int g_n_elems_static = 0;
static char g_house_root[PATH_BUF];

/* wraith-alpha-standard index nav state (see Elem.nav_index comment) */
static Elem *g_nav[MAX_ELEMS];
static int g_n_nav = 0;
/* Real, visible bug found live (2026-08-12, direct report: "no > is on
 * screen when it opens"): nav 1 used to ALWAYS be the chrome close
 * button, whose "[>N]" badge is deliberately suppressed (too small a
 * box to fit one - see draw_elem()'s own comment) in favor of just an
 * outline ring - so NO visible "[>N]" text existed anywhere on screen at
 * launch. Fixed properly in assign_nav_indices() (close moved to the
 * LAST nav index instead, per direct instruction), so nav 1 defaulting
 * here now lands on the first real content tab and shows immediately,
 * matching the taskbar/context menus always showing an obvious ">" on a
 * real row the instant they open. */
static int g_focus_nav = 1;   /* 1-based, matches nav_index numbering */
static int g_digit_accum = 0;
static int g_quit = 0;
static char g_last_key_label[32] = ""; /* see draw_chrome_bar()'s debug status line */

/* Chrome-bar drag-to-move, direct request 2026-08-12 ("window should be
 * draggable from header tab by mouse"). Now WM-managed with
 * _MOTIF_WM_HINTS decorations=0 (see main()'s own header comment) - a
 * real WM would normally supply titlebar-drag itself, but with
 * decorations off there's no WM-drawn titlebar to drag, so this needs
 * hand-rolled ButtonPress/MotionNotify/ButtonRelease drag, exact same
 * proven shape as 01.muchi-pals-🥚️-13.01/system/egg_window.c's own X11
 * drag block (ButtonPress records x_root/y_root, MotionNotify computes
 * the delta and XMoveWindow's, ButtonRelease ends it) - ported, not
 * reinvented. Scoped to the chrome bar only (not the whole window, since
 * tabs/buttons elsewhere need normal single-click activation). */
static int g_dragging = 0;
static int g_drag_last_x = 0, g_drag_last_y = 0;
/* Running window position, purely accumulated via deltas - matches
 * egg_window.c's own win_start_x/win_start_y exactly. Deliberately NOT
 * re-read from the server mid-drag (XGetWindowAttributes' x/y are
 * PARENT-relative, and a real WM-managed window may be reparented into
 * a frame even with decorations=0 - mixing that with root-relative
 * motion deltas would drift wrong). Initialized to the window's real
 * creation position in main(). */
static int g_win_x = 100, g_win_y = 100;
/* chat-hai's own forced window size (screen-relative, real fix for the
 * "apply_css() clobbers a one-time override every redraw" bug - see
 * layout_pass()'s own header comment where these are applied). 0 = not
 * yet computed (main() sets these once, from real screen dimensions,
 * before the first redraw). */
static int g_forced_win_w = 0, g_forced_win_h = 0;

static Elem *elem_new(const char *tag) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

/* ---------- tiny generic tag-tree parser ---------- */

static void skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

static void parse_attr_value(const char **p, char *out, size_t outsz) {
    skip_ws(p);
    if (**p != '"') { out[0] = '\0'; return; }
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') {
        if (n + 1 < outsz) out[n++] = **p;
        (*p)++;
    }
    out[n] = '\0';
    if (**p == '"') (*p)++;
}

static void apply_attr(Elem *e, const char *name, const char *val) {
    if (strcmp(name, "id") == 0) {
        snprintf(e->id, sizeof(e->id), "%s", val);
    } else if (strcmp(name, "class") == 0) {
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", val);
        char *tok = strtok(tmp, " ");
        while (tok && e->n_classes < CSS_MAX_CLASSES) {
            snprintf(e->classes[e->n_classes], sizeof(e->classes[0]), "%s", tok);
            e->n_classes++;
            tok = strtok(NULL, " ");
        }
    } else if (strcmp(name, "label") == 0) {
        snprintf(e->label, sizeof(e->label), "%s", val);
    } else if (strcmp(name, "onclick") == 0) {
        snprintf(e->onclick, sizeof(e->onclick), "%s", val);
    } else if (strcmp(name, "active") == 0) {
        e->active = (strcmp(val, "true") == 0);
    }
}

/* parses one element starting at '<' ; returns pointer just past this element
 * (including its closing tag, if any). */
static const char *parse_element(const char *p, Elem *parent) {
    skip_ws(&p);
    if (*p != '<') return p;
    p++;
    if (*p == '!') { /* comment <!-- ... --> */
        const char *end = strstr(p, "-->");
        return end ? end + 3 : p + strlen(p);
    }
    char tag[32]; size_t tn = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/') {
        if (tn + 1 < sizeof(tag)) tag[tn++] = *p;
        p++;
    }
    tag[tn] = '\0';
    Elem *e = elem_new(tag);
    e->parent = parent;
    if (parent && parent->n_children < MAX_CHILDREN) parent->children[parent->n_children++] = e;

    for (;;) {
        skip_ws(&p);
        if (*p == '/' && p[1] == '>') { p += 2; return p; }
        if (*p == '>') { p++; break; }
        if (!*p) return p;
        char attr[32]; size_t an = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '>' && *p != '/') {
            if (an + 1 < sizeof(attr)) attr[an++] = *p;
            p++;
        }
        attr[an] = '\0';
        skip_ws(&p);
        char val[256] = "";
        if (*p == '=') { p++; parse_attr_value(&p, val, sizeof(val)); }
        if (attr[0]) apply_attr(e, attr, val);
    }

    /* children, until matching close tag */
    for (;;) {
        skip_ws(&p);
        if (!*p) return p;
        if (p[0] == '<' && p[1] == '/') {
            const char *end = strchr(p, '>');
            return end ? end + 1 : p + strlen(p);
        }
        p = parse_element(p, e);
    }
}

static Elem *parse_chtpm(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    const char *p = buf;
    Elem *root = elem_new("__root__");
    /* skip any number of leading top-level comments (e.g. this file's own
     * doc-comment header) before parsing the real root element - a single
     * parse_element() call treats a comment as the whole top-level
     * construct and returns immediately after it, so without this loop
     * the real <window> tag was silently never reached (root stayed
     * empty). */
    for (;;) {
        skip_ws(&p);
        if (!*p) break;
        if (p[0] == '<' && p[1] == '!') {
            const char *end = strstr(p, "-->");
            p = end ? end + 3 : p + strlen(p);
            continue;
        }
        break;
    }
    if (*p == '<') parse_element(p, root);
    if (root->n_children > 0) root = root->children[0];
    free(buf);
    return root;
}

static Elem *find_by_tag(Elem *e, const char *tag) {
    if (!e) return NULL;
    if (strcmp(e->tag, tag) == 0) return e;
    for (int i = 0; i < e->n_children; i++) {
        Elem *r = find_by_tag(e->children[i], tag);
        if (r) return r;
    }
    return NULL;
}

/* Unlike find_by_tag(), disambiguates elements sharing a tag (e.g.
 * <text id="status"> and <text id="composer-text"> both being tag
 * "text" - see the real bug this fixed 2026-08-15: composer_sync()
 * was mutating "status"'s label via find_by_tag(window,"text")
 * matching the FIRST "text" element in document order, not the
 * composer). Real fixed-set of long-lived control elements (status/
 * toggle-pause/composer-text/send) should be looked up by id via this,
 * not by tag. */
static Elem *find_by_id(Elem *e, const char *id) {
    if (!e) return NULL;
    if (strcmp(e->id, id) == 0) return e;
    for (int i = 0; i < e->n_children; i++) {
        Elem *r = find_by_id(e->children[i], id);
        if (r) return r;
    }
    return NULL;
}

#define MAX_EVENTS 128
static char g_events[MAX_EVENTS][256]; /* was [96] with a hardcoded 72-char truncation - see load_ledger()'s own header comment for why that's gone now (real wrapping replaces it) */
static char g_speakers[MAX_EVENTS][32];
static char g_times[MAX_EVENTS][8]; /* "HH:MM" - short timestamp, restored 2026-08-15 (direct report: "we got rid of the timestamps... not good") */
static int g_n_events = 0;
static int g_selected_event = -1;
static int g_paused = 0;
/* "who's typing" (direct ask, 2026-08-15: "is it possible to show whos
 * 'thinking' (AKA TYPING) if waiting for a request?") - mirrors
 * chat_hai_loop.sh's own state/typing.txt (see that script's speak()
 * function): empty when nobody's mid-request, a persona name while
 * their qwen.sh call is in flight (the actually-slow part, ~20-40s per
 * this session's own logged timings). */
static char g_typing_name[64] = "";

/* ---------- data: sessions (2026-08-15, direct instruction: "we should
 * beable to add / delete new sessions (that will start fresh, new
 * memories)") — one independent .ledger file per session under
 * state/sessions/, matching ai-cell/open-hai's own real disk-persisted
 * deletable-history convention (chat-hai-design.md's own reference).
 * state/sessions/active.txt names which one is currently live; both this
 * renderer AND chat_hai_loop.sh (the actual chat scheduler) read it, so
 * switching sessions here takes effect in the running loop within one
 * round, not just visually. ---------- */

#define MAX_SESSIONS 32
static char g_session_names[MAX_SESSIONS][64]; /* basename, no .ledger ext */
static int g_n_sessions = 0;
static char g_active_session[64] = "main";

static void sessions_dir_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/&.hq-apps/chat-hai/state/sessions", g_house_root);
}

static void session_ledger_path(char *out, size_t outsz, const char *name) {
    char dir[PATH_BUF];
    sessions_dir_path(dir, sizeof(dir));
    snprintf(out, outsz, "%s/%s.ledger", dir, name);
}

static void active_session_path(char *out, size_t outsz) {
    char dir[PATH_BUF];
    sessions_dir_path(dir, sizeof(dir));
    snprintf(out, outsz, "%s/active.txt", dir);
}

/* One-time migration (first launch after this feature landed): if
 * state/sessions/ doesn't exist yet but the old single
 * state/transcript.ledger does, seed sessions/main.ledger from it so
 * existing conversation history isn't silently dropped. Safe to call
 * every startup - only acts once (checked via directory existence). */
static void migrate_legacy_ledger_if_needed(void) {
    char dir[PATH_BUF];
    sessions_dir_path(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) == 0) return; /* already migrated */
    mkdir(dir, 0755);
    char legacy[PATH_BUF], main_ledger[PATH_BUF];
    snprintf(legacy, sizeof(legacy), "%s/&.hq-apps/chat-hai/state/transcript.ledger", g_house_root);
    session_ledger_path(main_ledger, sizeof(main_ledger), "main");
    FILE *src = fopen(legacy, "r");
    if (src) {
        FILE *dst = fopen(main_ledger, "w");
        if (dst) {
            char buf[4096]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
            fclose(dst);
        }
        fclose(src);
    } else {
        FILE *dst = fopen(main_ledger, "w");
        if (dst) fclose(dst);
    }
    char active[PATH_BUF];
    active_session_path(active, sizeof(active));
    FILE *a = fopen(active, "w");
    if (a) { fprintf(a, "main\n"); fclose(a); }
}

/* Scans state/sessions/*.ledger into g_session_names[], and reads
 * active.txt into g_active_session. Cheap enough to call every frame
 * (small dir, small file) - matches this file's existing "reload from
 * disk every redraw" convention (load_ledger() itself). */
static void load_sessions_list(void) {
    g_n_sessions = 0;
    char dir[PATH_BUF];
    sessions_dir_path(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL && g_n_sessions < MAX_SESSIONS) {
            size_t len = strlen(de->d_name);
            if (len > 7 && strcmp(de->d_name + len - 7, ".ledger") == 0) {
                size_t namelen = len - 7;
                if (namelen >= sizeof(g_session_names[0])) namelen = sizeof(g_session_names[0]) - 1;
                strncpy(g_session_names[g_n_sessions], de->d_name, namelen);
                g_session_names[g_n_sessions][namelen] = '\0';
                g_n_sessions++;
            }
        }
        closedir(d);
    }
    /* simple lexical sort so the list is stable across frames (readdir
     * order is filesystem-dependent, not otherwise deterministic) */
    for (int i = 0; i < g_n_sessions - 1; i++)
        for (int j = i + 1; j < g_n_sessions; j++)
            if (strcmp(g_session_names[i], g_session_names[j]) > 0) {
                char tmp[64]; strcpy(tmp, g_session_names[i]);
                strcpy(g_session_names[i], g_session_names[j]);
                strcpy(g_session_names[j], tmp);
            }
    char active[PATH_BUF];
    active_session_path(active, sizeof(active));
    FILE *f = fopen(active, "r");
    if (f) {
        if (fgets(g_active_session, sizeof(g_active_session), f)) {
            char *nl = strchr(g_active_session, '\n');
            if (nl) *nl = '\0';
        }
        fclose(f);
    }
}

static void load_ledger(void); /* fwd decl, real def below - switch_session() needs it */

/* Writes state/sessions/active.txt = name and reloads. chat_hai_loop.sh
 * re-reads the same file at the top of each round (see that script's own
 * current_session()), so this takes effect for the running chat within
 * one round, not just for this window's own display. */
static void switch_session(const char *name) {
    snprintf(g_active_session, sizeof(g_active_session), "%s", name);
    char active[PATH_BUF];
    active_session_path(active, sizeof(active));
    FILE *f = fopen(active, "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }
    g_selected_event = -1;
    load_ledger();
    if (g_n_events > 0) g_selected_event = g_n_events - 1;
}

/* Real, working "add session" (direct instruction, 2026-08-15): creates
 * a fresh, empty ledger (new = genuinely fresh memories, not a copy of
 * any prior session) named by timestamp, and switches to it immediately
 * so the effect is visible right away, matching ai-cell's own
 * click-to-create-then-see-it feel. */
static void create_new_session(void) {
    char name[64];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    strftime(name, sizeof(name), "chat-%Y%m%d-%H%M%S", tmv);
    char path[PATH_BUF];
    session_ledger_path(path, sizeof(path), name);
    FILE *f = fopen(path, "w");
    if (f) {
        char t[32];
        strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", tmv);
        fprintf(f, "[%s] system: new session started | Trigger: chat-hai\n", t);
        fclose(f);
    }
    load_sessions_list();
    switch_session(name);
}

/* Real, working "delete session" — Backspace on a focused sidebar
 * session row (see handle_key()'s own new branch), matching ai-cell's
 * documented "Backspace on a sidebar row deletes it" convention
 * (chat-hai-design.md's own reference to that precedent). Refuses to
 * delete the LAST remaining session (a chat app with zero sessions is a
 * broken state, not an empty one) and always leaves the loop pointed at
 * a real, existing ledger afterward. */
static void delete_session(const char *name) {
    if (g_n_sessions <= 1) return;
    char path[PATH_BUF];
    session_ledger_path(path, sizeof(path), name);
    remove(path);
    load_sessions_list();
    if (strcmp(g_active_session, name) == 0 && g_n_sessions > 0) {
        switch_session(g_session_names[0]);
    } else {
        load_sessions_list();
    }
}

/* Loads the ACTIVE session's ledger (<house>/&.hq-apps/chat-hai/state/
 * sessions/<g_active_session>.ledger, see the sessions block above) into
 * g_events[] — the scrolling feed. REAL FIX 2026-08-15: previously read
 * a single hardcoded state/transcript.ledger regardless of session.
 * Lines use the master-ledger formula:
 *   [YYYY-MM-DD HH:MM:SS] <speaker>: <message> | Trigger: chat-hai
 * Display format: SHORT "HH:MM" timestamp kept (see g_times' own header
 * comment - a real regression this session accidentally dropped the
 * timestamp ENTIRELY instead of just shortening the on-screen display
 * of it, direct report: "not good i just wanted to make them smaller").
 * No more fixed-72-char content truncation either - inject_panel_feed()
 * now real-wraps each message to however many lines it actually needs
 * (see wrap_lines()), so keep the FULL message text here. */
static void load_ledger(void) {
    g_n_events = 0;
    char ledger[PATH_BUF];
    session_ledger_path(ledger, sizeof(ledger), g_active_session);
    FILE *f = fopen(ledger, "r");
    if (!f) return;
    /* REAL FIX 2026-08-15 (direct report: "chat isn't updating" - the
     * actual root cause after everything else this session): this loop
     * used to read from the START of the file and stop the moment
     * g_n_events hit MAX_EVENTS (128) - so once a session's ledger grew
     * past 128 lines (trivially easy after a day of testing - main's
     * was already at 175), every subsequent load_ledger() call re-read
     * the SAME first 128 (oldest) lines and NEVER reached anything
     * appended after that point. The mtime-poll fix earlier this
     * session (main loop's own stat()-and-reload) was firing correctly
     * on every new message, calling load_ledger() right on schedule -
     * it just kept re-loading the same stale head of the file every
     * time, which is why the feed looked completely frozen despite the
     * chat loop visibly still running. Real fix: count total lines
     * first, then skip to (total - MAX_EVENTS) before parsing, so this
     * always loads the TAIL (most recent), not the head. */
    long total_lines = 0;
    {
        char probe[512];
        while (fgets(probe, sizeof(probe), f)) total_lines++;
    }
    rewind(f);
    long skip = total_lines > MAX_EVENTS ? total_lines - MAX_EVENTS : 0;
    char line[512];
    while (skip-- > 0 && fgets(line, sizeof(line), f)) { /* discard */ }
    while (fgets(line, sizeof(line), f) && g_n_events < MAX_EVENTS) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *trig = strstr(line, " | Trigger: chat-hai");
        if (trig) *trig = '\0';
        /* pull "HH:MM" out of "[YYYY-MM-DD HH:MM:SS] ..." (chars 12-16
         * of a well-formed line) and advance content past the full
         * bracketed timestamp, same split point as before - only the
         * ON-SCREEN representation of the time got shorter, not gone. */
        char *content = line;
        g_times[g_n_events][0] = '\0';
        if (line[0] == '[') {
            char *bracket = strchr(line, ']');
            if (bracket && bracket[1] == ' ') {
                if (bracket - line >= 17) { /* "[YYYY-MM-DD HH:MM" is at least this long */
                    snprintf(g_times[g_n_events], sizeof(g_times[0]), "%.5s", line + 12);
                }
                content = bracket + 2;
            }
        }
        if (content[0]) {
            /* extract speaker name (text before the first ": ") for color-coding */
            char *colon = strstr(content, ": ");
            if (colon) {
                int speaker_len = colon - content;
                if (speaker_len > 0 && speaker_len < (int)sizeof(g_speakers[0]) - 1) {
                    strncpy(g_speakers[g_n_events], content, speaker_len);
                    g_speakers[g_n_events][speaker_len] = '\0';
                } else {
                    strcpy(g_speakers[g_n_events], "user");
                }
            } else {
                strcpy(g_speakers[g_n_events], "user");
            }
            /* REAL FIX 2026-08-15 (direct report: "explain why messages
             * cut off outside window instead of wrapping" - see
             * wrap_lines()/inject_panel_feed() for the actual fix): this
             * used to hard-truncate at a FIXED 72 characters regardless
             * of the panel's real pixel width, which is why long
             * messages visibly ran off the right edge of the window
             * instead of wrapping - a char-count guess has no relation
             * to actual rendered pixel width (font, per-character width,
             * and the panel's real width all vary). Full content is
             * kept here now; wrapping happens where real geometry is
             * known (inject_panel_feed(), at panel width, not a guess). */
            snprintf(g_events[g_n_events], sizeof(g_events[0]), "%s", content);
        } else continue;
        g_n_events++;
    }
    fclose(f);
}

/* REAL FIX 2026-08-15 (direct instruction: "sessions are on left ...
 * just like open-hai"): the sidebar (left column, per chat-hai.chtpm's
 * own <sidebar id="sessions">) is the SESSIONS list, not the chat feed
 * — a real, live layout bug had messages injected here instead, leaving
 * the actual message panel with no feed content at all (only its fixed
 * status/composer/send controls, stacked with no bounded scroll area,
 * which is what visually read as "input takes up the entire vertical
 * pane"). Real, working session list (2026-08-15 follow-up, direct
 * instruction: "we should beable to add / delete new sessions") — one
 * row per file in state/sessions/ (see load_sessions_list()), plus a
 * trailing "+ New Session" row (tag "newsession", disambiguated from
 * real session rows by tag, not id, in activate_elem()). Click a real
 * row to switch_session(); Backspace while it's focused deletes it (see
 * handle_key()'s own new branch) — matching ai-cell/open-hai's own
 * disk-persisted deletable-history convention this was built to mirror. */
static void inject_sessions(Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    load_sessions_list();
    /* REAL FIX 2026-08-15 (direct report: "THE MAIN/NEW+ FONTS ARE
     * BLACK ON DARK BACKGROUND") - class renamed from "data-item" to
     * "session-item": the old CSS rule targeting these
     * (".sessions .data-item") used a descendant combinator this
     * parser doesn't support (see chat-hai.css's own header comment on
     * the ".session-item" rule this class now matches) and silently
     * never applied, leaving these black. */
    for (int i = 0; i < g_n_sessions; i++) {
        Elem *item = elem_new("item");
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "session-item");
        item->n_classes = 1;
        snprintf(item->label, sizeof(item->label), "%s", g_session_names[i]);
        item->active = (strcmp(g_session_names[i], g_active_session) == 0);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
    Elem *newbtn = elem_new("newsession");
    newbtn->parent = sidebar;
    snprintf(newbtn->classes[0], sizeof(newbtn->classes[0]), "session-item");
    newbtn->n_classes = 1;
    snprintf(newbtn->label, sizeof(newbtn->label), "+ New Session");
    if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = newbtn;
}

/* Cached pointers to the panel's own fixed-position controls, captured
 * once right after parse_chtpm() (see main()) — inject_panel_feed()
 * rebuilds panel->children every layout_pass() with a variable-length
 * run of feed items followed by these 4 known elements, so they must
 * survive across n_children resets rather than being re-found by a
 * tag/id walk that would itself be searching the very array being
 * rebuilt. */
static Elem *g_status_elem = NULL;
static Elem *g_toggle_elem = NULL;
static Elem *g_speed_elem = NULL;
static Elem *g_composer_text_elem = NULL;
/* g_send_elem removed 2026-08-15 (direct instruction: "we dont need a
 * send button" - Enter already sends, see handle_key()'s Enter branch). */

/* Composes g_status_elem's label from BOTH g_paused and g_typing_name -
 * called from the toggle-pause click handler AND the main loop's own
 * typing.txt poll, so either one changing updates the same real status
 * text instead of the two clobbering each other's writes. */
static void update_status_label(void) {
    if (!g_status_elem) return;
    if (g_typing_name[0]) {
        snprintf(g_status_elem->label, sizeof(g_status_elem->label), "%s \xc2\xb7 %s typing\xe2\x80\xa6",
                 g_paused ? "[stopped]" : "[running]", g_typing_name);
    } else {
        snprintf(g_status_elem->label, sizeof(g_status_elem->label), "%s", g_paused ? "[stopped]" : "[running]");
    }
}

/* Rebuilds panel->children as [ up to n_visible tail feed items (oldest
 * of the visible window first, newest last) ..., status, toggle-pause,
 * composer-text, send ]. n_visible is computed by the CALLER
 * (layout_pass(), from the FIXED bottom-controls height reserved BEFORE
 * this runs — see that function's own comment) so there is no
 * chicken-and-egg between "how tall is the feed" and "how many items
 * are in it": the feed's pixel height is fixed by the bottom controls
 * alone, item count is derived FROM that height, never the reverse.
 * This is the same real fix class as ai-cell's own transcript_geom()
 * (composer height computed first, feed gets what's left) — see
 * chat-hai-design.md's "Real layout spec" section, ported here rather
 * than reinvented. */
/* Forward decls - apply_css()/measure_text_px()/wrap_lines() are defined
 * further down this file (they need dpy/screen/Xft, real definitions
 * near the rendering section), but inject_panel_feed() here needs them
 * for real per-message text wrapping (see wrap_lines()'s own header
 * comment). dpy/screen already have this exact "declared again earlier
 * than their real definition" pattern elsewhere in this file (both
 * appear a second time near the rendering section too) - same harmless
 * C tentative-declaration shape, not new precedent. */
static Display *dpy;
static int screen;
static void apply_css(Elem *e, int hover);
static int scaled(int base_px);
static int measure_text_px(const CssStyle *st, const char *text);
#define WRAP_MAX_LINES 8
#define WRAP_LINE_BUF 256
static int wrap_lines(const CssStyle *st, const char *text, int max_w, char lines[][WRAP_LINE_BUF]);

static void inject_panel_feed(Elem *panel, int n_visible) {
    if (!panel) return;
    panel->n_children = 0;
    if (n_visible < 0) n_visible = 0;
    if (g_n_events == 0) {
        Elem *item = elem_new("item");
        item->parent = panel;
        snprintf(item->label, sizeof(item->label), "(ledger empty — loop not running?)");
        if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = item;
    } else {
        /* REAL FIX 2026-08-15 (direct report: "explain why messages cut
         * off outside window instead of wrapping" - see wrap_lines()'s
         * own header comment): each message now becomes 1+ "item"
         * Elems (one per wrapped line), instead of always exactly one
         * possibly-overflowing Elem. n_visible counts LINES (unchanged
         * meaning from before - layout_pass()'s own per-child stacking
         * loop already treats each panel "item" child as one line-height
         * row, so no change needed there, just how many Elems this
         * function produces per logical message). Timestamp restored
         * (direct report: "we got rid of the timestamps... not good") -
         * prefixed on each message's FIRST wrapped line only. */
        int wrap_w = panel->w - scaled(8);
        if (wrap_w < 20) wrap_w = 20;
        Elem tmp_style_elem;
        memset(&tmp_style_elem, 0, sizeof(tmp_style_elem));
        snprintf(tmp_style_elem.tag, sizeof(tmp_style_elem.tag), "item");
        snprintf(tmp_style_elem.classes[0], sizeof(tmp_style_elem.classes[0]), "data-item");
        tmp_style_elem.n_classes = 1;
        apply_css(&tmp_style_elem, 0);

        typedef struct { int event_idx; int n; char lines[WRAP_MAX_LINES][WRAP_LINE_BUF]; } WrappedMsg;
        static WrappedMsg wm[MAX_EVENTS];
        int n_msgs = 0, total_lines = 0;
        for (int i = g_n_events - 1; i >= 0 && n_msgs < MAX_EVENTS; i--) {
            char full[400];
            if (g_times[i][0]) snprintf(full, sizeof(full), "%s %s", g_times[i], g_events[i]);
            else snprintf(full, sizeof(full), "%s", g_events[i]);
            int nl = wrap_lines(&tmp_style_elem.style, full, wrap_w, wm[n_msgs].lines);
            if (total_lines + nl > n_visible && n_msgs > 0) break; /* always keep at least the newest message, even if it alone overflows */
            wm[n_msgs].event_idx = i;
            wm[n_msgs].n = nl;
            total_lines += nl;
            n_msgs++;
            if (total_lines >= n_visible) break;
        }
        for (int m = n_msgs - 1; m >= 0; m--) { /* emit oldest-first, matching the panel's own top-to-bottom stacking */
            int i = wm[m].event_idx;
            for (int l = 0; l < wm[m].n; l++) {
                Elem *item = elem_new("item");
                if (!item) break;
                item->parent = panel;
                snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
                snprintf(item->classes[1], sizeof(item->classes[1]), "%s", g_speakers[i]);
                item->n_classes = 2;
                snprintf(item->label, sizeof(item->label), "%s", wm[m].lines[l]);
                item->active = (i == g_selected_event);
                if (panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = item;
            }
        }
    }
    /* Re-append the 4 cached fixed controls, in the SAME order
     * find_by_id()/find_by_tag() callers elsewhere in this file expect
     * (status first — composer_sync()'s sibling logic and the
     * toggle-pause handler both rely on status being found before any
     * other "text"-tagged element, see find_by_id()'s own header
     * comment for the bug this replaced). */
    if (g_status_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_status_elem;
    if (g_toggle_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_toggle_elem;
    if (g_speed_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_speed_elem;
    if (g_composer_text_elem && panel->n_children < MAX_CHILDREN) panel->children[panel->n_children++] = g_composer_text_elem;
}

/* ---------- layout: CSS overrides a small hand-rolled per-tag flow,
 * since v1 deliberately has no flex/grid engine (see plan) ---------- */

/* Order matches au11-hq/rpg-maker-database.html's own tab-bar exactly
 * (line 301-316) - real RPG Maker MV order, 15 tabs total. Direct
 * correction (2026-08-12): Common Events belongs right after Tilesets,
 * not last; "Terms" is its own 15th tab, separate from "Types" (both
 * exist in the mockup - not a typo/merge). */
static const char *TAB_LABELS[] = {
    "Actors", "Classes", "Skills", "Items", "Weapons", "Armors",
    "Enemies", "Troops", "States", "Animations", "Tilesets",
    "Common Events", "System", "Types", "Terms"
};
#define N_TABS 15
#define COMMON_EVENTS_TAB 11
static int g_current_tab = COMMON_EVENTS_TAB; /* the only wired tab */

static const CssSheet *g_sheet;

static void apply_css(Elem *e, int hover) {
    css_compute_style(g_sheet, e->tag, e->id[0] ? e->id : NULL, e->classes, e->n_classes, hover, &e->style);
}

/* X11/Xft globals - declared here (not down in the rendering section)
 * because layout now needs to MEASURE real font metrics, not guess a
 * fixed px-per-char width; that guess (7px/char) was the actual cause of
 * "big and jumbled" text - it didn't match whichever font XftFontOpenName
 * actually resolved, so boxes were sized wrong and labels overlapped. */
static Display *dpy;
static int screen;

/* user-defined UI scale, direct request: "even if the window needed to
 * be bigger... or even reading this from a std user defined font size
 * .pdl so user can adjust scale for readability/access". Shared across
 * all -hq apps (not taskbar-specific), same key=value .pdl convention
 * already used by khtpm_strip_parser.c's load_theme_opacity() (reads
 * #.desktop/livedesk_taskbar.pdl the same way). Applies to BOTH font
 * sizes and layout box sizes (chrome height, row heights, default window
 * size) so a bigger font never gets clipped by boxes that didn't grow
 * with it - text metrics are measured AFTER scaling (measure_text_px()
 * below), so nothing needs a second manual size fixup. */
static double g_font_scale = 1.0;

/* focus_grab: KISS hail-mary, direct instruction 2026-08-12 ("all that
 * focus stuff is overkill... keep it in a separate config/.pdl, do the
 * same as a last hail mary"). Studied egg_window.c (a real "context"
 * entity window, ALSO launched fresh from a click, confirmed reliably
 * keyboard-usable) and found it does ZERO focus/grab calls for its main
 * window - no XSetInputFocus, no XGrabKeyboard, nothing beyond plain
 * override_redirect + XMapWindow. Default flips to that same bare-
 * minimum behavior; the whole soft_focus()/XGrabKeyboard machinery
 * built earlier this session is kept but now OFF by default, toggleable
 * back on via this key without a rebuild if the simple path doesn't
 * actually fix it. */
static int g_focus_grab_enabled = 0;

static void load_font_scale(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/hq_ui.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        if (strcmp(line, "font_scale") == 0) {
            double v = atof(val);
            if (v >= 0.5 && v <= 3.0) g_font_scale = v; /* sane clamp - not a layout-breaking value */
        } else if (strcmp(line, "focus_grab") == 0) {
            g_focus_grab_enabled = atoi(val) != 0;
        } else if (strcmp(line, "window_x") == 0) {
            g_win_x = atoi(val);
        } else if (strcmp(line, "window_y") == 0) {
            g_win_y = atoi(val);
        }
    }
    fclose(f);
}

/* REAL FIX 2026-08-15 (direct instruction: "all window dims can be read
 * from .pdl isntead of hardcoded" - the standing !.HOUSE_STDS.md §A.7
 * rule this file already violated three separate rounds in a row while
 * iterating on screen position/size by hand-editing C constants and
 * rebuilding each time). Reads chat_hai_config.pdl's own
 * window_width/window_bottom_margin/window_right_margin/window_top_offset
 * keys (unscaled base pixels - g_font_scale still multiplies these the
 * same way it scales everything else, see that .pdl's own header
 * comment). Read ONCE at startup (unlike sleep_between, window geometry
 * doesn't need live mid-session reread) - called from main(), before
 * the screen-anchor block that consumes these values. */
static int g_cfg_window_width = 280;
static int g_cfg_bottom_margin = 50;
static int g_cfg_right_margin = 10;
static int g_cfg_top_offset = 100;

static void load_window_geometry_config(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/&.hq-apps/chat-hai/chat_hai_config.pdl", g_house_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char key[64], val[64];
        if (sscanf(line, " SECTION | %63[^|] | %63s", key, val) != 2) continue;
        char *k = key; while (*k == ' ') k++;
        char *ke = k + strlen(k); while (ke > k && ke[-1] == ' ') *--ke = '\0';
        int n = atoi(val);
        if (strcmp(k, "window_width") == 0) g_cfg_window_width = n;
        else if (strcmp(k, "window_bottom_margin") == 0) g_cfg_bottom_margin = n;
        else if (strcmp(k, "window_right_margin") == 0) g_cfg_right_margin = n;
        else if (strcmp(k, "window_top_offset") == 0) g_cfg_top_offset = n;
    }
    fclose(f);
}

static int scaled(int base_px) { return (int)(base_px * g_font_scale + 0.5); }

static int measure_text_px(const CssStyle *st, const char *text) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = scaled(st->has_font_size ? st->font_size : 12);
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=10");
    if (!f) return (int)strlen(text) * 8;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, f, (const FcChar8 *)text, (int)strlen(text), &ext);
    XftFontClose(dpy, f);
    return ext.width;
}

#define WRAP_MAX_LINES 8
#define WRAP_LINE_BUF 256

/* REAL FIX 2026-08-15 (direct report: "explain why messages cut off
 * outside window instead of wrapping" - see load_ledger()'s own header
 * comment for the removed fixed-72-char truncation this replaces):
 * real word-boundary wrapping at the ACTUAL pixel width available (via
 * measure_text_px(), not a character-count guess). Greedy: grows a
 * candidate line one word at a time, backtracks to the last word
 * boundary the moment it no longer fits max_w, same algorithm shape as
 * ai-cell's own wrap_text() (khtpm_ai_cell_render.c) - not reinvented,
 * chat-hai just didn't have its own copy of this until now. Returns the
 * number of lines written (capped at WRAP_MAX_LINES - a single message
 * that's still too long past that point gets a trailing "…" on the
 * last line rather than growing the feed unboundedly). */
static int wrap_lines(const CssStyle *st, const char *text, int max_w, char lines[][WRAP_LINE_BUF]) {
    int n = 0;
    const char *p = text;
    while (*p && n < WRAP_MAX_LINES) {
        const char *line_start = p;
        const char *last_space = NULL;
        const char *scan = p;
        char candidate[WRAP_LINE_BUF];
        candidate[0] = '\0';
        while (*scan) {
            const char *next = scan;
            while (*next && *next != ' ') next++;
            int seglen = (int)(next - line_start);
            if (seglen >= WRAP_LINE_BUF) seglen = WRAP_LINE_BUF - 1;
            char test[WRAP_LINE_BUF];
            memcpy(test, line_start, (size_t)seglen);
            test[seglen] = '\0';
            if (measure_text_px(st, test) > max_w && candidate[0]) break;
            snprintf(candidate, sizeof(candidate), "%s", test);
            if (*next == ' ') { last_space = next; scan = next + 1; } else { scan = next; break; }
        }
        (void)last_space;
        if (!candidate[0]) { /* single word wider than max_w - hard cut so we always make progress */
            size_t take = strlen(line_start);
            if (take > WRAP_LINE_BUF - 1) take = WRAP_LINE_BUF - 1;
            memcpy(candidate, line_start, take);
            candidate[take] = '\0';
        }
        snprintf(lines[n], WRAP_LINE_BUF, "%s", candidate);
        n++;
        p = line_start + strlen(candidate);
        while (*p == ' ') p++;
    }
    if (*p && n == WRAP_MAX_LINES) {
        /* still more text left after hitting the cap - mark truncation
         * on the last line rather than silently dropping the tail. */
        size_t l = strlen(lines[n - 1]);
        if (l < WRAP_LINE_BUF - 4) snprintf(lines[n - 1] + l, WRAP_LINE_BUF - l, "…");
    }
    return n;
}

/* Own drawn chrome bar (title + close), NOT a window-manager decoration -
 * same idea as wraith-alpha's own chrome row (ops/wraith_parser_alpha.c's
 * g_chrome_icons[]: nav 1 = title, icons after it, 'x' = CHROME_ACTION_
 * CLOSE), direct instruction: "we will create our own chrome bar and
 * title, ok? like in wraith-alpha". Kept to just title + close for this
 * app (no minimize/geom/context-menu - wraith-alpha's fuller icon set
 * isn't needed here). Window height grows by g_chrome_h on top of the
 * CSS/default content height, so nothing below has to shrink to fit it.
 * g_chrome_h (and every other layout constant in layout_pass() below) is
 * scaled by g_font_scale, not just font sizes - a bigger font with
 * same-size boxes just clips, per direct instruction: "even if the
 * window needed to be bigger". */
static int g_chrome_h = 26;
static Elem g_close_elem_storage;
static Elem *g_close_elem = &g_close_elem_storage;
static int g_close_x, g_close_y, g_close_w, g_close_h;

static void layout_pass(Elem *window) {
    /* Rewind the elem pool to the real .chtpm-declared baseline before
     * any dynamic injection below - see g_n_elems_static's own header
     * comment for the crash this fixes (pool exhaustion after enough
     * cumulative redraws, not tied to any specific click). Must happen
     * BEFORE inject_sessions()/inject_panel_feed() run further down. */
    g_n_elems = g_n_elems_static;
    apply_css(window, 0);
    /* REAL FIX 2026-08-15 (direct report: "its on right of screen but
     * still wide and stout instead of thin and long" - after the first
     * screen-anchor attempt): apply_css() just above re-reads
     * chat-hai.css's own fixed 900x700 into window->style EVERY call -
     * layout_pass() runs on every redraw(), so a one-time override set
     * in main() before the FIRST layout_pass() call got silently
     * clobbered back to the CSS default on the very next redraw
     * (relay input, a message arriving, anything). Force it here
     * instead, unconditionally, every call - g_forced_win_w/h are set
     * once in main() from real screen dimensions and never touched by
     * CSS again. */
    if (g_forced_win_w > 0) { window->style.has_width = 1; window->style.width = g_forced_win_w; }
    if (g_forced_win_h > 0) { window->style.has_height = 1; window->style.height = g_forced_win_h; }
    window->x = 0; window->y = 0;
    int default_w = scaled(900);
    int content_total_h = window->style.has_height ? window->style.height : scaled(600);

    Elem *tabbar = find_by_tag(window, "tabbar");
    Elem *sidebar = find_by_tag(window, "sidebar");
    Elem *panel = find_by_tag(window, "panel");

    int tabbar_h = scaled(30);
    int tab_widths[MAX_CHILDREN];
    int tabbar_natural_w = scaled(4);
    if (tabbar) {
        apply_css(tabbar, 0);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->active = (i == g_current_tab);
            apply_css(tab, 0);
            /* real measured width, not a guessed px/char - a mismatched
             * guess vs. the font XftFontOpenName actually resolved was
             * the root cause of overlapping/"jumbled" tab labels.
             * measure_text_px() already applies g_font_scale internally,
             * so this only needs to scale its own fixed padding/badge
             * allowance, not the measured part. Measured in this own
             * pre-pass (not while assigning x) so the window can grow to
             * fit ALL tabs first - 15 tabs (au11-hq/rpg-maker-database.
             * html's real count) don't fit the old fixed 900px default,
             * and this app has no flex-wrap engine to fall back on. */
            tab_widths[i] = measure_text_px(&tab->style, tab->label) + scaled(34); /* "[>NN]" badge + padding */
            tabbar_natural_w += tab_widths[i] + 1;
        }
    }
    window->w = window->style.has_width ? window->style.width : (tabbar_natural_w > default_w ? tabbar_natural_w : default_w);
    window->h = content_total_h + g_chrome_h;

    g_close_w = scaled(56); g_close_h = g_chrome_h - scaled(6); /* wide enough for "[>NN] x" - badge + label both now, see draw_elem()'s own comment */
    g_close_x = window->w - g_close_w - scaled(4);
    g_close_y = scaled(3);

    if (tabbar) {
        tabbar->x = 0; tabbar->y = g_chrome_h; tabbar->w = window->w; tabbar->h = tabbar_h;
        int tx = scaled(4);
        for (int i = 0; i < tabbar->n_children; i++) {
            Elem *tab = tabbar->children[i];
            tab->x = tx; tab->y = g_chrome_h + scaled(2); tab->w = tab_widths[i]; tab->h = tabbar_h - scaled(4);
            tx += tab_widths[i] + 1;
        }
    }

    int content_y = g_chrome_h + tabbar_h;
    int content_h = content_total_h - tabbar_h;
    /* REAL FIX 2026-08-15 (direct instruction: "sessions select can be
     * very small thin on right side not left") - was 210px on the LEFT
     * (matching ai-cell's own sidebar, per this file's earlier "copy
     * open-hai" convention); reversed per direct instruction. Feed is
     * now the main LEFT body, sessions a thin strip on the window's own
     * RIGHT edge (the window itself is already screen-right-anchored,
     * see main()'s own window_x default - this is the right edge of
     * THIS window, one level in from that). */
    int sidebar_w = scaled(90);

    if (g_current_tab != COMMON_EVENTS_TAB) {
        /* placeholder tabs: no sidebar/panel geometry needed, drawn as
         * one centered message directly against the window in render_pass() */
        return;
    }

    if (sidebar) {
        /* Re-injected every frame now, not just once at startup - since
         * g_n_elems rewinds to g_n_elems_static at the top of this
         * function every call (see that fix's own header comment), a
         * stale one-time injection's Elem pointers would otherwise get
         * silently overwritten by whatever inject_panel_feed() below
         * allocates next, corrupting the sessions list. Must run BEFORE
         * inject_panel_feed() so sessions claim the pool slots first. */
        inject_sessions(sidebar);
        apply_css(sidebar, 0);
        if (sidebar->style.has_width && !sidebar->style.width_is_pct) sidebar_w = sidebar->style.width;
        sidebar->x = window->w - sidebar_w; sidebar->y = content_y; sidebar->w = sidebar_w; sidebar->h = content_h;
        int iy = sidebar->y + scaled(4);
        int item_h = scaled(22);
        for (int i = 0; i < sidebar->n_children; i++) {
            Elem *item = sidebar->children[i];
            apply_css(item, 0);
            item->x = sidebar->x + scaled(4); item->y = iy; item->w = sidebar->w - scaled(8); item->h = item_h;
            iy += item->h;
        }
    }

    if (panel) {
        apply_css(panel, 0);
        int margin = scaled(8);
        panel->x = margin; /* LEFT edge of the window now - was sidebar_w + margin when sidebar lived on the left */
        panel->y = content_y + margin;
        panel->w = window->w - sidebar_w - margin * 2;
        panel->h = content_h - margin * 2;

        /* REAL FIX 2026-08-15 (chat-hai-design.md "Real layout spec",
         * ported from ai-cell's real transcript_geom()/draw_composer()
         * shape — khtpm_css_parser.c has no flex/box-model engine, see
         * that file's own "unrecognized properties, ignored" comment,
         * so chat-hai.css's flex-based bottom-pin was decorative
         * fiction and this must be pixel-computed here instead):
         * bottom-pinned control rows have a FIXED height computed
         * FIRST, independent of feed content; the feed then gets
         * exactly what's left above them. This is why the feed
         * correctly shrinks around fixed controls instead of a
         * control accidentally claiming the whole pane (the original,
         * real bug — a message-only sidebar list plus an unbounded
         * generic per-child stack in the panel with no fixed row
         * heights read as "input takes up the entire vertical pane"). */
        /* REAL FIX 2026-08-15 (direct report: "i didn't see 'thinking/
         * typing' in view yet... why isn't it rendered?"): status used
         * to share ONE row with both toggle-pause AND speed-toggle,
         * reserving a fixed scaled(174) for the two buttons - that math
         * was written when the window was still ~900px wide (§ROUND 1
         * of the screen-anchor fix). Once the window was narrowed to
         * ~280px unscaled (per "thin and long"), panel->w shrank to
         * ~218px real pixels - almost EXACTLY equal to that same fixed
         * 174*1.25=218px reservation, leaving status_elem's box at
         * effectively ZERO width. The typing indicator text was being
         * composed into the label correctly (confirmed via frame-
         * history's own typing= field) - it just had no visible box
         * left to draw into. Real fix: status gets its OWN full-width
         * row; toggle-pause + speed-toggle move to a SEPARATE row below
         * it, side by side. */
        int status_row_h = scaled(20);
        int button_row_h = scaled(22);
        int composer_row_h = scaled(30);
        int row_gap = scaled(4);
        int bottom_h = status_row_h + row_gap + button_row_h + row_gap + composer_row_h;
        int feed_top = panel->y + scaled(4);
        int feed_h = panel->h - scaled(4) - bottom_h;
        if (feed_h < 0) feed_h = 0;
        int item_h = scaled(18);
        int n_visible = item_h > 0 ? feed_h / item_h : 0;

        inject_panel_feed(panel, n_visible);

        int iy = feed_top;
        int bottom_y = panel->y + panel->h - bottom_h;
        int button_row_y = bottom_y + status_row_h + row_gap;
        int half_w = (panel->w - scaled(4) - scaled(8)) / 2; /* two buttons, one gap between, margins on both sides */
        for (int i = 0; i < panel->n_children; i++) {
            Elem *c = panel->children[i];
            apply_css(c, 0);
            if (strcmp(c->tag, "item") == 0) {
                /* feed message row - stacked top-down, fills the space
                 * ABOVE the fixed bottom controls (see bottom_h above),
                 * never the reverse - this is the actual scrolling-feed
                 * area chat-hai was missing entirely before this fix
                 * (messages used to live in the sidebar instead, see
                 * inject_sessions()'s own header comment). */
                c->x = panel->x + scaled(4);
                c->y = iy;
                c->w = panel->w - scaled(8);
                c->h = item_h;
                iy += item_h;
                continue;
            }
            if (c == g_status_elem) {
                /* own full-width row now - see this block's own header
                 * comment for why it was invisible before. */
                c->x = panel->x + scaled(4); c->y = bottom_y;
                c->w = panel->w - scaled(8); c->h = status_row_h;
            } else if (c == g_toggle_elem) {
                c->x = panel->x + scaled(4); c->y = button_row_y;
                c->w = half_w; c->h = button_row_h;
            } else if (c == g_speed_elem) {
                /* REAL, working GUI speed control (direct instruction,
                 * 2026-08-15: "can have an input in gui also" - for
                 * chat_hai_config.pdl's sleep_between setting). Cycles
                 * fixed presets (see activate_elem()'s own "speed-
                 * toggle" branch), writes chat_hai_config.pdl, which
                 * chat_hai_loop.sh re-reads every round (see that
                 * script's own sleep_between() function). */
                c->x = panel->x + scaled(4) + half_w + scaled(4); c->y = button_row_y;
                c->w = half_w; c->h = button_row_h;
            } else if (c == g_composer_text_elem) {
                /* the real cli-io composer row, pinned to the true
                 * bottom of the window - same "input anchored at the
                 * bottom, everything else flows above it" contract
                 * ai-cell/open-hai's own composer uses. Full row width
                 * since the send button (direct instruction: "we dont
                 * need a send button") is gone. */
                c->x = panel->x + scaled(4); c->y = bottom_y + status_row_h + row_gap + button_row_h + row_gap;
                c->w = panel->w - scaled(8); c->h = composer_row_h;
            }
        }
    }
}

/* wraith-alpha-standard index nav (ops/wraith_parser_alpha.c's own
 * digit_accum/do_jump/display_num convention, direct instruction: "wraith
 * alpha should be a huge inspiration for this"): every interactive
 * element gets a sequential 1-based number, assigned in the same order
 * they're laid out (tabs, then - if Common Events is open - sidebar
 * items, then panel buttons). Must run AFTER layout_pass() so it walks
 * exactly what's currently visible (placeholder tabs have no sidebar/
 * panel children to number). */
static void assign_nav_indices(Elem *window) {
    g_n_nav = 0;
    /* Chrome close control is now LAST, not nav 1 (direct instruction,
     * 2026-08-12: "u can give close button last nav index if that
     * helps") - its "[>N]" badge is deliberately suppressed (see
     * draw_elem()'s own comment, too small a box to fit one), so
     * defaulting focus there at launch left NO visible "[>N]" text
     * anywhere on screen. Content tabs now start at nav 1, matching the
     * taskbar/context menus always showing an obvious ">" on a real row
     * immediately. */
    Elem *tabbar = find_by_tag(window, "tabbar");
    if (tabbar) {
        for (int i = 0; i < tabbar->n_children && g_n_nav < MAX_ELEMS; i++) {
            Elem *tab = tabbar->children[i];
            tab->nav_index = ++g_n_nav;
            g_nav[g_n_nav - 1] = tab;
        }
    }
    if (g_current_tab == COMMON_EVENTS_TAB) {
        Elem *sidebar = find_by_tag(window, "sidebar");
        if (sidebar) {
            for (int i = 0; i < sidebar->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *item = sidebar->children[i];
                item->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = item;
            }
        }
        Elem *panel = find_by_tag(window, "panel");
        if (panel) {
            /* REAL FIX 2026-08-15 (direct report: "open chat user input
             * is supposed to have nav [] and number so its relay and
             * human index accessible. why did we deviate from these
             * stds?") — this loop used to skip anything not tag
             * "button", which silently dropped composer-text (tag
             * "text") from ever getting a nav_index/[>]N badge, unlike
             * ai-cell/open-hai's own composer which IS a numbered
             * cli-io target. The blanket "buttons only" rule was
             * copied from the events-hq/db-hq template this file
             * started as (see this file's own header comment) where
             * the panel really does only ever contain buttons - never
             * updated when chat-hai's composer-text landed. Explicit
             * allowlist now: buttons AND the composer field, nothing
             * else (status text and feed message rows still correctly
             * get no nav_index - they're not digit-jump targets). */
            for (int i = 0; i < panel->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *c = panel->children[i];
                int navigable = (strcmp(c->tag, "button") == 0) || (c == g_composer_text_elem);
                if (!navigable) { c->nav_index = 0; continue; }
                c->nav_index = ++g_n_nav;
                g_nav[g_n_nav - 1] = c;
            }
        }
    }
    if (g_n_nav < MAX_ELEMS) {
        g_close_elem->nav_index = ++g_n_nav;
        g_nav[g_n_nav - 1] = g_close_elem;
    }
    if (g_focus_nav < 1) g_focus_nav = 1;
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
}

/* ---------- rendering ---------- */

static Display *dpy;
static Window win;
static int screen;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Colormap cmap;

static unsigned long alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') {
        if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel;
    } else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) {
        return c.pixel;
    }
    return BlackPixel(dpy, screen);
}

static XftColor xft_color(const char *spec) {
    XftColor xc;
    XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b;
        sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc);
    return xc;
}

static XftFont *font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = scaled(st->has_font_size ? st->font_size : 12);
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    return f ? f : XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=10");
}

static void draw_elem(Elem *e, int hover_id_hash) {
    (void)hover_id_hash;
    if (e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.bg_color));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (e->style.has_border_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.border_color));
        int bw = e->style.has_border_width ? e->style.border_width : 1;
        for (int i = 0; i < bw; i++)
            XDrawRectangle(dpy, buf, gc, e->x + i, e->y + i, e->w - 1 - 2 * i, e->h - 1 - 2 * i);
    }
    if (strcmp(e->tag, "tab") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#ffffff"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    /* REAL BUG FOUND + DISABLED 2026-08-15 (direct report: "why is last
     * chat highlighted? i cant read it. get rid of that for now") - the
     * newest ledger line auto-becomes g_selected_event on every load
     * (see load_ledger()'s own callers), which set item->active=1 on
     * that message's Elem(s), which painted this LIGHT BLUE (#cce5ff)
     * background behind it - but feed message text color comes from
     * per-speaker CSS classes (chat-hai.css's .data-item.<speaker>
     * rules), all light/pastel colors chosen to read against the app's
     * DARK background (#16181f/#1e2130). Light pastel text on a light
     * blue highlight = unreadable, every single time (always the newest
     * message, since that's what auto-selects). Disabled per direct
     * instruction rather than reworked - a real "selected message" UI
     * (if wanted later) needs a per-speaker-aware contrast choice, not
     * a single fixed highlight color; not in scope right now. */
    /* wraith-alpha-standard focus ring: the currently-focused navigable
     * element gets a highlighted outline, matching wraith_parser_alpha.c's
     * "[>]" focus prefix convention (adapted to a visible box here since
     * this is a graphical renderer, not the text-grid wraith-alpha draws
     * into). */
    if (e->nav_index > 0 && e->nav_index == g_focus_nav) {
        XSetForeground(dpy, gc, alloc_pixel("#ff8c00"));
        XDrawRectangle(dpy, buf, gc, e->x - 1, e->y - 1, e->w + 1, e->h + 1);
    }
    int pad = e->style.has_padding ? e->style.padding : 4;
    int label_x = e->x + pad;
    /* nav-index badge: bracket-wrapped, moving ">" focus marker - matches
     * the taskbar/toolbar's own convention (khtpm_taskbar_manager.c's
     * hq_focus highlight; wraith_parser_alpha.c's "[>]"/"[ ]" prefix
     * this whole nav system was ported from). "[>3]" when focused,
     * "[ 3]" otherwise, in its own small muted font so it reads as a
     * toolbar index badge, not run into the label's own text. */
    /* Direct correction 2026-08-12 ("x close isn't getting a number...
     * everything gets a number") - the close button used to be
     * special-cased out of the badge (its box was too small and the
     * badge pushed the label off-screen, see the earlier "off screen to
     * the right" fix). Real fix is a wider box (g_close_w, see
     * layout_pass()) and a shorter label ("x" not "[x]", since the
     * badge itself now supplies the brackets) instead of an exception -
     * every nav item gets a number, no special cases. */
    if (e->nav_index > 0) {
        char badge[16];
        int focused = (e->nav_index == g_focus_nav);
        /* REAL FIX 2026-08-12, direct correction ("db-hq and hai are
         * using nav index in not quite the std the std is [].<#> not
         * [<#>]"): verified against the actual real reference
         * (1.TPMOS_c_+rmmp.0103.0001/projects/wraith-alpha/ops/
         * wraith_parser_alpha.c ~line 2221-2224/2283) - the bracket
         * holds ONLY the state glyph (`[^]`/`[>]`/`[]`/`[ ]`), the
         * number is a SEPARATE suffix drawn after the closing bracket
         * with a trailing period (`pref + "%d." `, e.g. `[>]1.`), NOT
         * embedded inside the brackets as `[>1]`. This was wrong
         * everywhere in this house's own khtpm/-hq family until now -
         * see !.HOUSE_STDS.md #22's own correction for why this must
         * not drift back. */
        snprintf(badge, sizeof(badge), "[%c]%d.", focused ? '>' : ' ', e->nav_index);
        char numspec[48];
        snprintf(numspec, sizeof(numspec), "DejaVu Sans Mono:pixelsize=%d", scaled(9));
        XftFont *numfont = XftFontOpenName(dpy, screen, numspec);
        if (!numfont) { snprintf(numspec, sizeof(numspec), "DejaVu Sans:pixelsize=%d", scaled(9)); numfont = XftFontOpenName(dpy, screen, numspec); }
        XftColor numcol = xft_color(focused ? "#ff8c00" : "#9a9a9a");
        XGlyphInfo numext;
        XftTextExtentsUtf8(dpy, numfont, (const FcChar8 *)badge, (int)strlen(badge), &numext);
        int numy = e->y + (e->h + numfont->ascent - numfont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &numcol, numfont, label_x, numy, (const FcChar8 *)badge, (int)strlen(badge));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
        label_x += numext.width + 5;
        XftFontClose(dpy, numfont);
    }
    if (e->label[0]) {
        XftFont *font = font_for(&e->style);
        XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : "#000000");
        XGlyphInfo extents;
        XftTextExtentsUtf8(dpy, font, (const FcChar8 *)e->label, (int)strlen(e->label), &extents);
        int ty = e->y + (e->h + font->ascent - font->descent) / 2;
        if (ty < e->y + font->ascent) ty = e->y + font->ascent + pad / 2;
        XftDrawStringUtf8(xftdraw_buf, &col, font, label_x, ty, (const FcChar8 *)e->label, (int)strlen(e->label));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
        XftFontClose(dpy, font);
    }
}

/* absolute-positioned children (the floating block-title) are painted in
 * a later pass than their parent, per the design doc's own suggested
 * approach - this walk draws non-title children first, titles last. */
static void render_tree(Elem *e, int depth) {
    if (depth == 0) draw_elem(e, 0);
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) continue; /* deferred */
        draw_elem(c, 0);
        render_tree(c, depth + 1);
    }
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) draw_elem(c, 0);
    }
}

static void render_placeholder_tab(Elem *window) {
    char pspec[48];
    snprintf(pspec, sizeof(pspec), "DejaVu Sans:pixelsize=%d", scaled(12));
    XftFont *font = XftFontOpenName(dpy, screen, pspec);
    XftColor col = xft_color("#888888");
    char msg[64];
    snprintf(msg, sizeof(msg), "%s — (coming soon)", TAB_LABELS[g_current_tab]);
    XGlyphInfo extents;
    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)msg, (int)strlen(msg), &extents);
    int tx = (window->w - extents.width) / 2;
    int ty = window->h / 2;
    XftDrawStringUtf8(xftdraw_buf, &col, font, tx, ty, (const FcChar8 *)msg, (int)strlen(msg));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
    XftFontClose(dpy, font);
}

/* Real, documented bug class (!.HOUSE_STDS.md F-19): under this house's
 * Mutter/XWayland environment, a brand-new override_redirect window does
 * NOT reliably receive real keyboard input on bare mapping alone -
 * XGetInputFocus can report success while KeyPress events never arrive.
 * This is almost certainly why arrows/digit-jump looked broken (direct
 * report: "doesn't have > focus arrow move or digit jump yet") despite
 * handle_key()'s own logic being correct and already proven working
 * through the relay (which bypasses X input focus entirely, so it never
 * hit this). Fix is the SAME proven raise-then-focus-then-flush sequence
 * already used by khtpm_strip_parser.c's taskbar_soft_focus() - ported,
 * not reinvented, per that bug report's own explicit standard ("don't
 * invent a fresh focus mechanism without first checking whether an
 * already-proven pattern solves it").
 *
 * DIAGNOSTIC (also ported, khtpm_strip_parser.c's own g_has_real_focus):
 * XSetInputFocus() is a REQUEST, not a guarantee - this tracks whether
 * the window ACTUALLY has focus right now via real FocusIn/FocusOut
 * events, the only authoritative source. If this never goes true despite
 * soft_focus() being called, KeyPress events genuinely never reach this
 * process - a different, deeper problem than db-hq's own key-handling
 * logic (which is separately already proven correct via the relay). */
static int g_has_real_focus = 0;

static void soft_focus(void) {
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    XFlush(dpy);
}

/* Real fix (found live, 2026-08-12): a grab taken ONCE at startup isn't
 * enough for a long-lived window - a fresh FocusIn immediately followed
 * by FocusOut appeared after a genuine physical click, meaning the grab
 * had already been lost/preempted sometime after launch with nothing to
 * recover it. tp_desktop_window.c's popups never hit this because
 * they're short-lived and re-created (thus re-grabbed) fresh every time
 * one opens - db-hq is one persistent window across its whole session,
 * so it must re-request the grab on every interaction instead, not just
 * once. Keyboard-only (see the call site's own note on why not
 * XGrabPointer too). */
static void grab_keyboard_retry(void) {
    for (int attempt = 0; attempt < 5; attempt++) {
        int rc = XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (rc == GrabSuccess) break;
        XSync(dpy, False);
        usleep(5000);
    }
}

static Elem *g_window;

/* RGB compose→present refactor (2026-08-12, direct instruction: "we
 * should do db to rgb refactor. the need being auditability"). Proven
 * first on a throwaway test binary (!.khtpm-rgb-refactor.md's own
 * "Phase 0" - compose buffer vs. presented-window readback confirmed
 * BYTE-IDENTICAL two independent ways before trusting this pattern on
 * real code). Real change here, not a rewrite: `redraw()` still
 * composes into `buf` (the offscreen Pixmap) exactly as before via Xft/
 * Xlib - only the PRESENT step changes, from `XCopyArea` (Pixmap→Window
 * blit, no portable byte buffer ever exists) to deriving one real
 * `XImage` via `XGetImage` and presenting THAT via `XPutImage` (proven
 * pixel-identical in Phase 0). g_frame_rgb is the persistent, single-
 * source-of-truth 3-byte-per-pixel copy of "what's actually on screen
 * right now" - dump_frame_png() just writes THIS out directly instead
 * of doing its own separate XGetImage capture (the old, more fragile
 * two-different-capture-paths shape) - this IS the auditability the
 * refactor was for: one real buffer, inspectable at any time, not
 * derived fresh and possibly-differently each time something wants to
 * look at the frame. */
static unsigned char *g_frame_rgb = NULL;
static int g_frame_w = 0, g_frame_h = 0;

/* debug PNG dump - see the header comment above the stb_image_write.h
 * include. RGB refactor (2026-08-12): writes the single persistent
 * `g_frame_rgb` buffer redraw() already derived for the real on-screen
 * present - no separate XGetImage capture of its own anymore. This IS
 * the auditability point of the refactor: what gets dumped is
 * byte-for-byte the same buffer that was actually presented, not a
 * fresh, possibly-different second capture. Bound to 'p' - not part of
 * the normal render loop, purely an on-demand debug aid. */
static void dump_frame_png(void) {
    if (!g_frame_rgb || g_frame_w <= 0 || g_frame_h <= 0) {
        fprintf(stderr, "db-hq: dump_frame_png: no frame composed yet\n");
        return;
    }
    int ok = stbi_write_png("/tmp/chat-hai-frame.png", g_frame_w, g_frame_h, 3, g_frame_rgb, g_frame_w * 3);
    fprintf(stderr, ok ? "chat-hai: wrote /tmp/chat-hai-frame.png (%dx%d)\n" : "chat-hai: dump_frame_png: write failed\n", g_frame_w, g_frame_h);
}

/* Own chrome bar (title + close) - see layout_pass()'s CHROME_H comment
 * for the wraith-alpha precedent. Drawn unconditionally, every redraw,
 * regardless of which tab is open - matches wraith-alpha's own chrome
 * row staying fixed while body content underneath changes. */
static void draw_chrome_bar(void) {
    XSetForeground(dpy, gc, alloc_pixel("#2b2b2b"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, g_chrome_h);

    char tspec[48];
    snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d:bold", scaled(10));
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (!titlefont) { snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=%d", scaled(10)); titlefont = XftFontOpenName(dpy, screen, tspec); }
    XftColor titlecol = xft_color("#eeeeee");
    /* legacy taskbar's own "^" convention (direct instruction 2026-08-12:
     * "legacy toolbar had a '^' indicator near digits, i noticed we lost
     * that but we could add it here" / "'^' indicating window had
     * focus") - real, ground-truth g_has_real_focus (set only by an
     * actual FocusIn event, "the only authoritative source" per
     * khtpm_strip_parser.c's own F-19 diagnostic this was ported from),
     * not a guess or a request-was-sent flag. */
    char title[16];
    snprintf(title, sizeof(title), "chat-hai %s", g_has_real_focus ? "^" : " ");
    int ty = (g_chrome_h + titlefont->ascent - titlefont->descent) / 2;
    XftDrawStringUtf8(xftdraw_buf, &titlecol, titlefont, scaled(8), ty, (const FcChar8 *)title, (int)strlen(title));
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
    XftFontClose(dpy, titlefont);

    g_close_elem->x = g_close_x; g_close_elem->y = g_close_y;
    g_close_elem->w = g_close_w; g_close_elem->h = g_close_h;
    snprintf(g_close_elem->label, sizeof(g_close_elem->label), "x");
    css_style_init(&g_close_elem->style);
    g_close_elem->style.has_border_color = 1;
    snprintf(g_close_elem->style.border_color, sizeof(g_close_elem->style.border_color), "%s",
             g_close_elem->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
    g_close_elem->style.has_border_width = 1; g_close_elem->style.border_width = 1;
    g_close_elem->style.has_fg_color = 1;
    snprintf(g_close_elem->style.fg_color, sizeof(g_close_elem->style.fg_color), "#eeeeee");
    draw_elem(g_close_elem, 0);

    /* Debug status line, direct request 2026-08-12 ("we could show
     * digits in header like tb") - shows the last raw key this PROCESS
     * actually received and the current digit accumulator, live, so
     * it's visually obvious (not just in a log file) whether a real
     * keypress ever reaches this window at all vs. reaches it but
     * doesn't visibly move focus for some other reason - two very
     * different bugs that look identical from the outside otherwise. */
    char dbg[96];
    snprintf(dbg, sizeof(dbg), "Key:%s  Digits:%d  Focus:%d/%d  RealFocus:%s",
             g_last_key_label[0] ? g_last_key_label : "(none yet)",
             g_digit_accum, g_focus_nav, g_n_nav, g_has_real_focus ? "yes" : "no");
    char dspec[48];
    snprintf(dspec, sizeof(dspec), "DejaVu Sans:pixelsize=%d", scaled(9));
    XftFont *dfont = XftFontOpenName(dpy, screen, dspec);
    if (dfont) {
        XftColor dcol = xft_color("#88cc88");
        XGlyphInfo dext;
        XftTextExtentsUtf8(dpy, dfont, (const FcChar8 *)dbg, (int)strlen(dbg), &dext);
        int dx = g_window->w - g_close_w - scaled(12) - dext.width;
        int dy = (g_chrome_h + dfont->ascent - dfont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &dcol, dfont, dx, dy, (const FcChar8 *)dbg, (int)strlen(dbg));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &dcol);
        XftFontClose(dpy, dfont);
    }
}

/* REAL FIX 2026-08-15 (direct instruction: "you should check it with
 * injection and framehistory.txt (we dont need a png dump to see if
 * frames are updating from chat)") — matches the exact convention
 * khtpm_strip_parser.c already uses for the taskbar
 * (#.desktop/khtpm_strip_frame_history.txt): one text line appended
 * per redraw(), so an agent (or a human) can `tail -f` real state
 * (event count, active session, paused flag, last message) without
 * ever needing a screenshot. This is genuinely faster to verify against
 * than a PNG dump for anything that's really a DATA question ("is the
 * feed updating") rather than a real LAYOUT question ("does it look
 * right") - use this for the former, PNG dumps for the latter. */
static void append_frame_history(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/#.desktop/chat_hai_frame_history.txt", g_house_root);
    FILE *f = fopen(path, "a");
    if (!f) return;
    const char *last = g_n_events > 0 ? g_events[g_n_events - 1] : "";
    char last_short[80];
    snprintf(last_short, sizeof(last_short), "%.60s", last);
    fprintf(f, "session=%s n_events=%d paused=%d typing=%s focus_nav=%d/%d win_x=%d win_y=%d win_w=%d win_h=%d last=\"%s\"\n",
            g_active_session, g_n_events, g_paused, g_typing_name[0] ? g_typing_name : "-", g_focus_nav, g_n_nav, g_win_x, g_win_y,
            g_window ? g_window->w : 0, g_window ? g_window->h : 0, last_short);
    fclose(f);
}

static void redraw(void) {
    layout_pass(g_window);
    assign_nav_indices(g_window);
    XSetForeground(dpy, gc, alloc_pixel(g_window->style.has_bg_color ? g_window->style.bg_color : "#ececec"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, g_window->h);
    if (g_current_tab != COMMON_EVENTS_TAB) {
        Elem *tabbar = find_by_tag(g_window, "tabbar");
        if (tabbar) { draw_elem(tabbar, 0); render_tree(tabbar, 1); }
        render_placeholder_tab(g_window);
    } else {
        render_tree(g_window, 0);
    }
    draw_chrome_bar();

    /* COMPOSE→PRESENT split (see g_frame_rgb's own header comment) -
     * derive the one real portable buffer from what was just drawn into
     * `buf`, present via XPutImage (proven pixel-identical to the old
     * XCopyArea path in Phase 0), and keep a persistent RGB copy for
     * dump_frame_png()/'p' to write out directly - no second, separate
     * capture path anymore. */
    XSync(dpy, False);
    int w = g_window->w, h = g_window->h;
    XImage *frame = XGetImage(dpy, buf, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    if (frame) {
        XPutImage(dpy, win, gc, frame, 0, 0, 0, 0, (unsigned)w, (unsigned)h);
        /* standard 24/32bpp TrueColor byte layout, not the (zeroed on a
         * bare Pixmap) mask fields - same fix already established for
         * this app's own debug dump. */
        if (g_frame_w != w || g_frame_h != h) {
            free(g_frame_rgb);
            g_frame_rgb = malloc((size_t)w * h * 3);
            g_frame_w = w; g_frame_h = h;
        }
        if (g_frame_rgb) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    unsigned long px = XGetPixel(frame, x, y);
                    size_t o = ((size_t)y * w + x) * 3;
                    g_frame_rgb[o] = (unsigned char)((px >> 16) & 0xff);
                    g_frame_rgb[o + 1] = (unsigned char)((px >> 8) & 0xff);
                    g_frame_rgb[o + 2] = (unsigned char)(px & 0xff);
                }
            }
        }
        XDestroyImage(frame);
    } else {
        /* fall back to the old direct blit if XGetImage ever fails, so
         * a capture problem degrades to "no audit buffer this frame,"
         * never "no picture at all." */
        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)w, (unsigned)h, 0, 0);
    }
    XFlush(dpy);
    append_frame_history();
}

/* ---------- hit testing / click dispatch ---------- */

static Elem *hit_test(Elem *e, int px, int py) {
    for (int i = e->n_children - 1; i >= 0; i--) {
        Elem *r = hit_test(e->children[i], px, py);
        if (r) return r;
    }
    if (px >= e->x && px < e->x + e->w && py >= e->y && py < e->y + e->h) return e;
    return NULL;
}

/* Composer: the user's jump-in line. Typed chars accumulate here (the
 * panel <text id="composer-text"> mirrors it); Enter appends the line to
 * the master-ledger formula and the next persona turn answers it. */
#define COMPOSER_BUF 128
static char g_composer[COMPOSER_BUF] = "";
static int g_composer_len = 0;

static void composer_sync(void) {
    /* REAL FIX 2026-08-15: was find_by_tag(g_window, "text"), which
     * matches the FIRST "text"-tagged element in document order — that's
     * "status", not "composer-text" (both share tag "text"). This
     * silently mutated the status line instead of the composer every
     * keystroke. See find_by_id()'s own header comment. */
    if (g_composer_text_elem) snprintf(g_composer_text_elem->label, sizeof(g_composer_text_elem->label), "> %s_", g_composer);
}

static void send_composer(void) {
    while (g_composer_len > 0 && g_composer[g_composer_len - 1] == ' ') g_composer_len--;
    g_composer[g_composer_len] = '\0';
    if (g_composer_len == 0) { composer_sync(); redraw(); return; }
    char t[32];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", tmv);
    char ledger[PATH_BUF];
    session_ledger_path(ledger, sizeof(ledger), g_active_session);
    FILE *f = fopen(ledger, "a");
    if (f) {
        fprintf(f, "[%s] user: %s | Trigger: chat-hai\n", t, g_composer);
        fclose(f);
    }
    g_composer_len = 0;
    g_composer[0] = '\0';
    load_ledger();
    if (g_n_events > 0) g_selected_event = g_n_events - 1;
    /* feed re-injection now happens inside layout_pass() (called by
     * redraw() below), from the current g_events/g_n_events just
     * reloaded above - no separate injection call needed here anymore,
     * see inject_panel_feed()'s own header comment. */
    composer_sync();
    redraw();
}

/* PRE-EXISTING BUG FOUND 2026-08-15: send_cli_prompt() referenced an
 * undeclared g_cli_prompts[] and is never called from anywhere in this
 * file - the binary running earlier this session was stale (built
 * before this dead code landed, never rebuilt since; see
 * chat-hai-design.md's own layout-fix section for the same "always
 * fully rebuild+restart" lesson). Declaring the missing array (all
 * unset for now) is the minimal fix to make this compile again; wiring
 * real cli-io quick-prompts (digits 1-9, like ai-cell/open-hai's own
 * numbered-shortcut composer) is unstarted, separate future work. */
static const char *g_cli_prompts[10] = {0};

static void send_cli_prompt(int digit) {
    if (digit < 1 || digit > 9 || !g_cli_prompts[digit]) return;
    char t[32];
    time_t now = time(NULL);
    struct tm *tmv = localtime(&now);
    strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", tmv);
    char ledger[PATH_BUF];
    session_ledger_path(ledger, sizeof(ledger), g_active_session);
    FILE *f = fopen(ledger, "a");
    if (f) {
        fprintf(f, "[%s] user: <%d> %s | Trigger: chat-hai\n", t, digit, g_cli_prompts[digit]);
        fclose(f);
    }
    load_ledger();
    if (g_n_events > 0) g_selected_event = g_n_events - 1;
    redraw();
}

/* shared dispatch for both mouse clicks and keyboard index-activation
 * (Enter on the focused nav_index) - wraith-alpha's own convention is
 * that a numbered element behaves identically whichever input method
 * reaches it. */
static void activate_elem(Elem *hit) {
    if (!hit) return;
    if (strcmp(hit->tag, "closebtn") == 0) {
        g_quit = 1;
        return;
    }
    if (strcmp(hit->tag, "tab") == 0) {
        for (int i = 0; i < N_TABS; i++) if (strcmp(hit->label, TAB_LABELS[i]) == 0) { g_current_tab = i; break; }
        redraw();
        return;
    }
    if (strcmp(hit->tag, "newsession") == 0) {
        create_new_session();
        redraw();
        return;
    }
    if (strcmp(hit->tag, "item") == 0) {
        /* Disambiguate by parent, NOT just tag - real sessions
         * (sidebar) and feed messages (panel) both use tag "item" (see
         * inject_sessions()/inject_panel_feed()'s own header comments).
         * A sidebar item click switches the active session (real
         * effect - chat_hai_loop.sh re-reads active.txt too, see
         * switch_session()); a panel item click just selects which
         * message line is highlighted, as before. */
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        if (hit->parent == sidebar) {
            switch_session(hit->label);
            redraw();
            return;
        }
        for (int i = 0; i < g_n_events; i++) if (strcmp(g_events[i], hit->label) == 0) { g_selected_event = i; break; }
        redraw();
        return;
    }
    if (strcmp(hit->id, "send") == 0) {
        send_composer();
        return;
    }
    if (strcmp(hit->id, "toggle-pause") == 0) {
        /* REAL FIX 2026-08-15 (direct instruction: "the start stop of
         * chat should be of the logic of the ai lan call itself if need
         * be" - after "i still dont get results from start/stop" with
         * the PRIOR pkill -STOP/-CONT approach): SIGSTOP on
         * chat_hai_loop.sh's own bash PROCESS does not reliably freeze
         * a curl call already in flight inside a command-substitution
         * subshell - "stopped" chat kept producing replies mid-flight.
         * Real fix: a plain control FILE (state/paused.txt) that the
         * loop script checks in a wait-loop right before EVERY qwen.sh/
         * curl call (see chat_hai_loop.sh's own speak() function,
         * "REAL FIX 2026-08-15" comment there) - guarantees zero new
         * LAN calls while paused, no OS-signal timing races. */
        g_paused = !g_paused;
        char pause_path[PATH_BUF];
        snprintf(pause_path, sizeof(pause_path), "%s/&.hq-apps/chat-hai/state/paused.txt", g_house_root);
        FILE *pf = fopen(pause_path, "w");
        if (pf) { fprintf(pf, "%d\n", g_paused ? 1 : 0); fclose(pf); }
        if (g_toggle_elem) {
            snprintf(g_toggle_elem->label, sizeof(g_toggle_elem->label), "%s", g_paused ? "Start" : "Stop");
        }
        update_status_label();
        redraw();
        return;
    }
    if (strcmp(hit->id, "speed-toggle") == 0) {
        /* REAL, working GUI speed control (direct instruction,
         * 2026-08-15: "can have an input in gui also" - for the same
         * sleep_between setting chat_hai_config.pdl now exposes as a
         * hand-editable file). Cycles a fixed preset list and writes
         * the .pdl chat_hai_loop.sh's own sleep_between() function
         * reads every round - no restart needed, matches that
         * function's own header comment. */
        static const int presets[] = { 2, 4, 6, 12, 20 };
        static const int n_presets = (int)(sizeof(presets) / sizeof(presets[0]));
        char cfg_path[PATH_BUF];
        snprintf(cfg_path, sizeof(cfg_path), "%s/&.hq-apps/chat-hai/chat_hai_config.pdl", g_house_root);
        int cur = 6, idx = 2; /* default matches chat_hai_config.pdl's own default */
        FILE *rf = fopen(cfg_path, "r");
        if (rf) {
            char line[256];
            while (fgets(line, sizeof(line), rf)) {
                if (strstr(line, "sleep_between")) {
                    char *bar2 = strrchr(line, '|');
                    if (bar2) cur = atoi(bar2 + 1);
                    break;
                }
            }
            fclose(rf);
        }
        for (int i = 0; i < n_presets; i++) if (presets[i] == cur) { idx = i; break; }
        int next_val = presets[(idx + 1) % n_presets];
        FILE *wf = fopen(cfg_path, "w");
        if (wf) {
            fprintf(wf, "# chat_hai_config.pdl - live-edited by the speed-toggle GUI button (chat_hai_hq_render.c)\nSECTION | sleep_between | %d\n", next_val);
            fclose(wf);
        }
        if (g_speed_elem) {
            snprintf(g_speed_elem->label, sizeof(g_speed_elem->label), "Speed: %ds", next_val);
        }
        redraw();
        return;
    }
}

static void handle_click(int px, int py) {
    /* close button lives in the chrome bar, outside window's own tag
     * tree (it's synthetic, not parsed from dashboard.chtpm) - check it
     * before the tree walk. */
    if (px >= g_close_elem->x && px < g_close_elem->x + g_close_elem->w &&
        py >= g_close_elem->y && py < g_close_elem->y + g_close_elem->h) {
        g_focus_nav = g_close_elem->nav_index;
        activate_elem(g_close_elem);
        return;
    }
    Elem *hit = hit_test(g_window, px, py);
    if (!hit) return;
    if (hit->nav_index > 0) g_focus_nav = hit->nav_index;
    activate_elem(hit);
}

/* wraith-alpha-standard digit-accumulation key handling (ports
 * ops/wraith_parser_alpha.c's digit_accum/do_jump/Enter-activates
 * convention): digits move focus live as they're typed (do_jump), Enter
 * activates the focused element, any other key resets the accumulator. */
static void handle_key(KeySym ks, char ch) {
    if (ch == 'p') { dump_frame_png(); return; }
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (g_digit_accum > 0 && g_digit_accum <= g_n_nav) g_focus_nav = g_digit_accum;
        g_digit_accum = 0;
        if (g_composer_len > 0) { send_composer(); return; }
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) {
        if (g_digit_accum > 0) { g_digit_accum = 0; return; }
        if (g_composer_len > 0) { g_composer_len = 0; g_composer[0] = '\0'; composer_sync(); redraw(); return; }
        g_quit = 1; /* no WM chrome/close button (override_redirect) - Escape closes instead */
        return;
    }
    if (ks == XK_BackSpace) {
        /* Real, working "delete session" (direct instruction, 2026-08-15:
         * "we should beable to add / delete new sessions") - matches
         * ai-cell/open-hai's own documented "Backspace on a sidebar row
         * deletes it" convention (chat-hai-design.md's own reference).
         * Only fires when the CURRENTLY FOCUSED nav element is a real
         * sidebar session row (not the "+ New Session" button, not a
         * panel feed message) - falls through to composer-edit
         * otherwise, so this never eats a Backspace the user meant for
         * their in-progress message. */
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) {
            Elem *f = g_nav[g_focus_nav - 1];
            Elem *sidebar = find_by_tag(g_window, "sidebar");
            if (f && f->parent == sidebar && strcmp(f->tag, "item") == 0) {
                delete_session(f->label);
                redraw();
                return;
            }
        }
        if (g_composer_len > 0) {
            g_composer[--g_composer_len] = '\0';
            composer_sync();
            redraw();
        }
        return;
    }
    if (ch >= '1' && ch <= '9') {
        /* Digits 1-9: send the composer text (cli-io shortcut, like open-hai) */
        send_composer();
        return;
    }
    if (ch == '0') {
        g_digit_accum = 0;
        return;
    }
    if (ks == XK_Up || ks == XK_Left) {
        if (g_focus_nav > 1) g_focus_nav--;
        g_digit_accum = 0;
        redraw();
        return;
    }
    if (ks == XK_Down || ks == XK_Right || ks == XK_Tab) {
        if (g_focus_nav < g_n_nav) g_focus_nav++;
        g_digit_accum = 0;
        redraw();
        return;
    }
    /* printable chars (not digits, not 'p' receipt) go to the composer */
    if (ch >= 32 && ch <= 126) {
        if (g_composer_len < COMPOSER_BUF - 1) {
            g_composer[g_composer_len++] = ch;
            g_composer[g_composer_len] = '\0';
        }
        composer_sync();
        redraw();
        return;
    }
    g_digit_accum = 0;
}

/* Agent relay (au11-hq/_.0.aigent-testing-k9.txt's documented "third
 * option" for raw-Xlib programs: "give the program its OWN file-relay
 * polling loop, additive alongside its existing XNextEvent() loop"):
 * <house_root>/#.desktop/db_hq_agent_relay.txt, one decimal ASCII code
 * per line (48-57 digits, 13 Enter, 27 Escape, 32-126 other printable) -
 * SAME contract as khtpm_strip_parser.c's poll_agent_relay() (never
 * replay backlog on first poll, resync-not-replay on truncation, leave a
 * partial trailing line for next time), ported line-for-line from that
 * function since it's already the proven, documented shape for this
 * exact problem. Dispatches through the SAME handle_key() the real
 * KeyPress handler uses (see dispatch_key_code()'s own header comment in
 * khtpm_strip_parser.c for why sharing beats duplicating). No XTest, no
 * shared input focus with a real human on the same display. */
static long g_relay_cursor = -1;

static void relay_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%s/#.desktop/chat_hai_agent_relay.txt", g_house_root);
}

static void dispatch_relay_code(int code) {
    if (code == 13) handle_key(XK_Return, 0);
    else if (code == 27) handle_key(XK_Escape, 0);
    else if (code == 8) handle_key(XK_BackSpace, 0);
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
        if (!nl) break; /* partial line at EOF - wait for the rest next poll */
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
    if (argc < 3) {
        fprintf(stderr, "usage: %s <house_root> <chtpm_path>\n", argv[0]);
        return 1;
    }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);

    load_font_scale(); /* #.desktop/hq_ui.pdl's font_scale key - see load_font_scale()'s own header comment */
    load_window_geometry_config(); /* chat_hai_config.pdl's window_* keys - see that function's own header comment */
    g_chrome_h = scaled(26);

    memset(g_close_elem, 0, sizeof(*g_close_elem));
    snprintf(g_close_elem->tag, sizeof(g_close_elem->tag), "closebtn");

    migrate_legacy_ledger_if_needed(); /* one-time: seed sessions/main.ledger from the old single-ledger file, see its own header comment */
    load_sessions_list(); /* populates g_active_session from active.txt before the first load_ledger() below needs it */
    load_ledger();
    if (g_n_events > 0) g_selected_event = g_n_events - 1;

    Elem *window = parse_chtpm(argv[2]);
    if (!window) {
        fprintf(stderr, "db-hq: failed to parse %s\n", argv[2]);
        return 1;
    }
    g_window = window;

    g_n_elems_static = g_n_elems; /* baseline for the pool-rewind fix, see g_n_elems_static's own header comment */

    /* Cache the panel's 4 fixed control elements BEFORE anything ever
     * clears panel->n_children (inject_panel_feed() rebuilds that array
     * every layout_pass() - these pointers must survive that, see
     * inject_panel_feed()'s own header comment). Must happen right after
     * parse, while the .chtpm's own static children are still exactly
     * as written in the file. */
    Elem *panel0 = find_by_tag(window, "panel");
    g_status_elem = panel0 ? find_by_id(panel0, "status") : NULL;
    g_toggle_elem = panel0 ? find_by_id(panel0, "toggle-pause") : NULL;
    g_speed_elem = panel0 ? find_by_id(panel0, "speed-toggle") : NULL;
    g_composer_text_elem = panel0 ? find_by_id(panel0, "composer-text") : NULL;

    /* No startup inject_sessions()/inject_panel_feed() call here - both
     * now run every frame from inside layout_pass() itself (right after
     * its own g_n_elems rewind), so the first real redraw() below
     * populates them correctly; a separate call here would just get
     * immediately overwritten and risks looking like it's still needed
     * standalone when it no longer is. */
    composer_sync(); /* composer text element starts empty */

    char css_path[PATH_BUF];
    snprintf(css_path, sizeof(css_path), "%s", argv[2]);
    char *dot = strrchr(css_path, '.');
    if (dot) snprintf(dot, sizeof(css_path) - (size_t)(dot - css_path), ".css");
    static CssSheet sheet;
    memset(&sheet, 0, sizeof(sheet));
    css_load(css_path, &sheet);
    g_sheet = &sheet;

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "db-hq: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);

    /* REAL FIX 2026-08-15, ROUND 2 (direct report: "its on right of
     * screen but still wide and stout instead of thin and long. and it
     * should be 50px lower"): round 1 set window->style.width/height
     * directly, ONCE, right here - but layout_pass()'s own
     * apply_css(window,0) call re-reads chat-hai.css's fixed 900x700
     * into window->style EVERY SINGLE CALL (every redraw - relay input,
     * a new chat message arriving, anything), silently reverting this
     * one-time override back to "wide and stout" on the very next
     * redraw after the first. Real fix: g_forced_win_w/h (set once here,
     * from real screen dimensions) are applied INSIDE layout_pass()
     * itself, AFTER its own apply_css() call, every single time - see
     * that function's own header comment. Width narrowed further
     * (280px, genuinely thin - 420 still read as "stout") and Y offset
     * increased by 50px per direct instruction (was scaled(40), now
     * +50 more) to clear the desktop's own header bar. */
    /* ROUND 4 (direct instruction: "all window dims can be read from
     * .pdl isntead of hardcoded") - all four numbers now come from
     * load_window_geometry_config() (chat_hai_config.pdl), not hand-
     * edited C constants. See that function's own header comment. */
    {
        int screen_w = DisplayWidth(dpy, screen);
        int screen_h = DisplayHeight(dpy, screen);
        g_forced_win_w = scaled(g_cfg_window_width);
        g_forced_win_h = screen_h - scaled(g_cfg_top_offset) - scaled(g_cfg_bottom_margin);
        window->style.has_width = 1; window->style.width = g_forced_win_w;
        window->style.has_height = 1; window->style.height = g_forced_win_h;
        g_win_x = screen_w - g_forced_win_w - scaled(g_cfg_right_margin);
        g_win_y = scaled(g_cfg_top_offset);
    }

    layout_pass(window);
    int ww = window->w, wh = window->h;

    /* override_redirect, no WM decoration - same convention every khtpm
     * window uses (khtpm_strip_parser.c's win/hq_win/popup_win all set
     * CWOverrideRedirect the same way; see au11-hq/HQML-DESIGN+PLANS.md's
     * "Window Chrome Convention" note). No WM titlebar means no WM close
     * button either - Escape (with no digit pending) closes the window
     * instead, see handle_key(). */
    /* Real architecture fix (2026-08-12, direct instruction "yes do
     * that" after finding: real physical clicks reliably reach this
     * window - ButtonPress works fine - but FocusIn never fires no
     * matter what X11-level focus/grab calls are made). Root cause:
     * this system has org.gnome.mutter focus-change-on-pointer-rest =
     * true, an automatic Mutter WM focus policy - but override_redirect
     * windows are explicitly EXEMPT from window-manager focus handling
     * by X11 protocol definition, so Mutter never considers this window
     * for real focus transfer AT ALL, regardless of clicking or any
     * client-side XSetInputFocus/XGrabKeyboard call. The taskbar's own
     * override_redirect windows only "get away with it" because they
     * grab initial focus once, early in the session, and mostly never
     * need it back - not because override_redirect genuinely supports
     * reliable focus under this compositor.
     *
     * Fix: stop being override_redirect. Become a normally WM-MANAGED
     * window instead (so Mutter applies its real focus policy - the
     * same one that already works for every ordinary app on this
     * desktop), and suppress the visible title bar/border via the
     * standard _MOTIF_WM_HINTS "no decorations" hint below instead of
     * via override_redirect - a widely-supported way to get "managed
     * but borderless" rather than "borderless but unmanaged and
     * therefore focus-exempt". */
    XSetWindowAttributes swa;
    swa.background_pixel = WhitePixel(dpy, screen);
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                         CopyFromParent, InputOutput, CopyFromParent,
                         CWBackPixel | CWEventMask, &swa);
    {
        /* _MOTIF_WM_HINTS: flags=MWM_HINTS_DECORATIONS(2), decorations=0
         * - hides the title bar/border on any WM that honors Motif hints
         * (Mutter does), without exempting the window from WM focus
         * management the way override_redirect does. */
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 }; /* flags, functions, decorations, input_mode, status */
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);

        /* WM_HINTS input=True - ICCCM's own way for a client to declare
         * it expects/accepts keyboard input via the normal input-focus
         * model, checked by real window managers when deciding whether
         * to grant click-to-focus at all. */
        XWMHints *wmhints = XAllocWMHints();
        if (wmhints) {
            wmhints->flags = InputHint;
            wmhints->input = True;
            XSetWMHints(dpy, win, wmhints);
            XFree(wmhints);
        }

        /* Now a real managed window - register WM_DELETE_WINDOW so a WM
         * or Alt+F4 asks nicely instead of killing the process outright
         * (own [x]/Escape close paths still work regardless). */
        Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, win, &wm_delete, 1);

        /* PPosition: tell the WM the x/y passed to XCreateWindow are a
         * REAL placement request, not a default hint. Without this, Mutter
         * (org.gnome.mutter) treats them as unspecified and auto-places
         * the window (it was landing at arbitrary spots like 148,54 or
         * 198,104 instead of the hq_ui.pdl window_x/window_y - direct
         * report 2026-08-13 "stats and db-hq window opens too high on
         * desktop, underneath tb directly"). PPosition is the standard
         * way every WM honors an explicitly-requested screen position. */
        XSizeHints *shints = XAllocSizeHints();
        if (shints) {
            shints->flags = PPosition;
            shints->x = g_win_x;
            shints->y = g_win_y;
            XSetWMNormalHints(dpy, win, shints);
            XFree(shints);
        }
    }
    {
        /* MUST be "MuchiverseLivedesk", not a db-hq-specific class - real
         * root cause found (studied tp_desktop_window.c's open_context_
         * menu(), $.crypts/enable_xwayland_grabs.sh): Mutter's Wayland
         * compositor restricts XGrabKeyboard from XWayland clients by
         * default (org.gnome.mutter.wayland xwayland-allow-grabs=false,
         * a real security policy, not a bug), and xwayland-grab-access-
         * rules allowlists by WM_CLASS - this house's rule already
         * allowlists exactly "MuchiverseLivedesk" (confirmed:
         * `gsettings get org.gnome.mutter.wayland xwayland-grab-access-
         * rules` -> ['MuchiverseLivedesk']). A different class here would
         * make the grab below silently fail exactly like tp_desktop_
         * window.c's own original bug. */
        XClassHint *ch = XAllocClassHint();
        if (ch) {
            ch->res_name = (char *)"MuchiverseLivedesk";
            ch->res_class = (char *)"MuchiverseLivedesk";
            XSetClassHint(dpy, win, ch);
            XFree(ch);
        }
    }
    XMapRaised(dpy, win);
    /* sync g_win_x/g_win_y to wherever the WM actually placed it (real
     * position was 198,104 in testing, not the requested 100,100) - a
     * ONE-TIME read via XGetWindowAttributes right after mapping is
     * exactly the coordinate space XMoveWindow itself expects
     * (parent-relative), so this is safe here even though re-reading it
     * repeatedly DURING a drag would not be (see g_win_x's own header
     * comment on why dragging uses pure accumulated deltas instead). */
    XSync(dpy, False);
    { XWindowAttributes wa; if (XGetWindowAttributes(dpy, win, &wa)) { g_win_x = wa.x; g_win_y = wa.y; } }
    /* focus_grab=0 (default, see load_font_scale()'s own comment on
     * g_focus_grab_enabled): KISS hail-mary - egg_window.c's own entity
     * window does NONE of this (no XSetInputFocus, no XGrabKeyboard) and
     * reliably works despite ALSO launching fresh from a click, so try
     * matching that bare-minimum behavior exactly before assuming more
     * machinery is the answer. focus_grab=1 keeps the earlier grab+retry
     * approach (ported from tp_desktop_window.c's open_context_menu())
     * available as a fallback without needing a rebuild. */
    if (g_focus_grab_enabled) {
        grab_keyboard_retry();
        soft_focus();
    }
    /* drain any stale Button/KeyPress already queued for this window id
     * before it existed (X11 can recycle a just-destroyed window's ID for
     * the next XCreateWindow call - same real race tp_desktop_window.c's
     * own comment documents) so a leftover event can't phantom-activate
     * something the instant this window maps. */
    XSync(dpy, False);
    { XEvent stale_ev; while (XCheckWindowEvent(dpy, win, ButtonPressMask | KeyPressMask, &stale_ev)) { } }

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)ww, (unsigned)wh, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);

    redraw();

    /* headless verification aid: argv[3]=="--dump-and-exit" dumps one
     * frame and quits immediately, for environments with no key-sender
     * tool (xdotool/xte) available to press 'p' interactively. */
    if (argc > 3 && strcmp(argv[3], "--dump-and-exit") == 0) {
        dump_frame_png();
        g_quit = 1;
    }

    time_t last_ledger_mtime = 0; /* local, not global - persists across loop iterations, that's all it needs */
    while (!g_quit) {
        /* REAL FIX 2026-08-15 (direct report: "i dont see chat moving"):
         * this loop only ever redrew on relay input or a real X11 event
         * (Expose/click/key) - chat_hai_loop.sh writes new messages to
         * the ledger completely independently, on its own SLEEP_BETWEEN
         * timer, with no signal to this process at all. A user just
         * WATCHING the window (not typing, not clicking) would never
         * see the feed advance, matching chat-hai-design.md's own
         * stated intent ("a constantly scrolling chat feed") exactly
         * NOT happening. Real fix: stat() the active session's ledger
         * every tick (cheap - one small file, already-open select()
         * timeout paces this to ~150ms) and reload+redraw on any mtime
         * change, regardless of source (loop, or a session switch). */
        char ledger_check[PATH_BUF];
        session_ledger_path(ledger_check, sizeof(ledger_check), g_active_session);
        struct stat lst;
        if (stat(ledger_check, &lst) == 0 && lst.st_mtime != last_ledger_mtime) {
            last_ledger_mtime = lst.st_mtime;
            load_ledger();
            if (g_n_events > 0) g_selected_event = g_n_events - 1;
            redraw();
        }
        /* "who's typing" poll (direct ask, 2026-08-15: "is it possible
         * to show whos 'thinking' (AKA TYPING) if waiting for a
         * request?") - same cheap-stat-and-reread shape as the ledger
         * poll just above; chat_hai_loop.sh writes the persona's name
         * here right before its own (slow) qwen.sh call and clears it
         * right after, see that script's speak() function. */
        {
            char typing_path[PATH_BUF];
            snprintf(typing_path, sizeof(typing_path), "%s/&.hq-apps/chat-hai/state/typing.txt", g_house_root);
            char cur_typing[64] = "";
            FILE *tf = fopen(typing_path, "r");
            if (tf) {
                if (fgets(cur_typing, sizeof(cur_typing), tf)) {
                    char *nl = strchr(cur_typing, '\n');
                    if (nl) *nl = '\0';
                }
                fclose(tf);
            }
            if (strcmp(cur_typing, g_typing_name) != 0) {
                snprintf(g_typing_name, sizeof(g_typing_name), "%s", cur_typing);
                update_status_label();
                redraw();
            }
        }
        /* relay poll every loop tick, independent of X events - same
         * shape as khtpm_strip_parser.c's own main loop (poll_agent_
         * relay() call before the select()). */
        if (poll_agent_relay() > 0 && !g_quit) redraw();
        if (g_quit) break;

        fd_set fds;
        FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, 150000 }; /* 150ms, matches this app's own scale (small window, infrequent redraws) */
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                redraw();
            } else if (ev.type == ButtonPress) {
                /* focus_grab=0 (default): egg_window.c's own entity
                 * window does nothing focus-related on click either -
                 * matching that bare-minimum KISS behavior. focus_grab=1
                 * keeps the grab+retry / right-click-force-focus
                 * machinery available without a rebuild if the simple
                 * path turns out not to be enough. */
                if (g_focus_grab_enabled) {
                    grab_keyboard_retry();
                    soft_focus();
                }
                /* chrome-bar drag start - see g_dragging's own header
                 * comment. Only when the press lands in the chrome bar
                 * itself and NOT on the close button (so [x] still just
                 * closes on click, doesn't start a drag first). */
                if (ev.xbutton.button == 1 && ev.xbutton.y < g_chrome_h &&
                    !(ev.xbutton.x >= g_close_elem->x && ev.xbutton.x < g_close_elem->x + g_close_elem->w &&
                      ev.xbutton.y >= g_close_elem->y && ev.xbutton.y < g_close_elem->y + g_close_elem->h)) {
                    g_dragging = 1;
                    g_drag_last_x = ev.xbutton.x_root;
                    g_drag_last_y = ev.xbutton.y_root;
                }
                if (ev.xbutton.button != 3) handle_click(ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
                g_dragging = 0;
            } else if (ev.type == MotionNotify) {
                if (g_dragging) {
                    int dx = ev.xmotion.x_root - g_drag_last_x;
                    int dy = ev.xmotion.y_root - g_drag_last_y;
                    g_win_x += dx; g_win_y += dy;
                    XMoveWindow(dpy, win, g_win_x, g_win_y);
                    g_drag_last_x = ev.xmotion.x_root;
                    g_drag_last_y = ev.xmotion.y_root;
                }
            } else if (ev.type == KeyPress) {
                char buf8[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf8, sizeof(buf8) - 1, &ks, NULL);
                buf8[n > 0 ? n : 0] = '\0';
                /* ground-truth log: this fires the INSTANT a real X11
                 * KeyPress reaches this process, before any of handle_
                 * key()'s own nav logic runs - if this line never
                 * appears in the log despite real physical typing, the
                 * problem is 100% confirmed upstream of this app (X11/
                 * Xwayland/Mutter focus delivery), not this app's own
                 * key-handling code, which was already separately
                 * proven correct via the relay. */
                const char *kname = XKeysymToString(ks);
                snprintf(g_last_key_label, sizeof(g_last_key_label), "%s", kname ? kname : (buf8[0] ? buf8 : "?"));
                fprintf(stderr, "db-hq: REAL KeyPress received: keysym=%s char=%c\n", kname ? kname : "?", buf8[0] ? buf8[0] : '?');
                handle_key(ks, buf8[0]);
                redraw(); /* so the debug status line updates even if handle_key's own branch didn't already redraw */
            } else if (ev.type == FocusIn) {
                g_has_real_focus = 1;
                fprintf(stderr, "db-hq: FocusIn (real keyboard focus confirmed)\n");
                redraw(); /* live-update the "^" title indicator, not just on next keypress */
            } else if (ev.type == FocusOut) {
                g_has_real_focus = 0;
                fprintf(stderr, "db-hq: FocusOut (keyboard focus lost)\n");
                redraw();
            } else if (ev.type == ClientMessage) {
                /* WM_DELETE_WINDOW - now a real managed window (see the
                 * XSetWMProtocols() call in main()), so a WM/Alt+F4 can
                 * send this instead of just killing the process. */
                Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
                if ((Atom)ev.xclient.data.l[0] == wm_delete) g_quit = 1;
            }
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    XftDrawDestroy(xftdraw_buf);
    XFreePixmap(dpy, buf);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    KtbState ktb;
    ktb_init(&ktb, g_house_root);
    ktb_quit_and_save(&ktb);

    return 0;
}
