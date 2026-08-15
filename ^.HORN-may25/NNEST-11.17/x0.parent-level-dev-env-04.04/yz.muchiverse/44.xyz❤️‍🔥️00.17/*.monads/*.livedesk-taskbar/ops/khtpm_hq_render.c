/* khtpm_hq_render.c — db-hq: first proof of the HQML CSS styling layer
 * (au11-hq/HQML-DESIGN+PLANS.md Phase 1), scoped to the Common Events
 * section per au11-hq/rpg-maker-database.html and au11-hq/todo-a12.txt.
 *
 * Standalone binary, deliberately NOT a modification of
 * khtpm_strip_parser.c/khtpm_strip_layout.c - the taskbar's own renderer
 * is untouched so nothing here can regress it. Own window, own event
 * loop, own tiny generic tag-tree parser (reads db-hq's own .chtpm tag
 * vocabulary: window/tabbar/tab/sidebar/item/panel/title/text/button),
 * styled via khtpm_css_parser.c against a matching .css file.
 *
 * Usage: khtpm_hq_render.+x <house_root> <chtpm_path>
 * common_events are read/written at <house_root>/common_events/<name>/,
 * the same GLOBAL (not session-scoped) location db-ez uses - see
 * livedesk_build_db_common_events_menu() in khtpm_taskbar_manager.c. */
#include "khtpm_css_parser.h"
#include "khtpm_taskbar_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
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

/* ---------- data: common_events listing (GLOBAL, house_root-wide) ---------- */

#define MAX_EVENTS 128
static char g_events[MAX_EVENTS][64];
static int g_n_events = 0;
static int g_selected_event = -1;

static void load_common_events(void) {
    g_n_events = 0;
    char ce_root[PATH_BUF];
    snprintf(ce_root, sizeof(ce_root), "%s/common_events", g_house_root);
    if (access(ce_root, F_OK) != 0) mkdir(ce_root, 0755);
    DIR *d = opendir(ce_root);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && g_n_events < MAX_EVENTS) {
        if (de->d_name[0] == '.') continue;
        char ep[PATH_BUF];
        snprintf(ep, sizeof(ep), "%s/%s", ce_root, de->d_name);
        struct stat st;
        if (stat(ep, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(g_events[g_n_events], sizeof(g_events[0]), "%s", de->d_name);
        g_n_events++;
    }
    closedir(d);
    for (int i = 0; i < g_n_events - 1; i++)
        for (int j = i + 1; j < g_n_events; j++)
            if (strcmp(g_events[j], g_events[i]) < 0) {
                char t[64]; snprintf(t, sizeof(t), "%s", g_events[i]);
                snprintf(g_events[i], sizeof(g_events[i]), "%s", g_events[j]);
                snprintf(g_events[j], sizeof(g_events[j]), "%s", t);
            }
}

/* replaces the sidebar's single <placeholder id="common_events_rows"/>
 * child with one dynamically-built "item" element per common event
 * (mirrors ${common_events_rows} substitution from the design doc, done
 * structurally here instead of as a string template). */
static void inject_sidebar_items(Elem *sidebar) {
    if (!sidebar) return;
    sidebar->n_children = 0;
    for (int i = 0; i < g_n_events; i++) {
        Elem *item = elem_new("item");
        item->parent = sidebar;
        snprintf(item->classes[0], sizeof(item->classes[0]), "data-item");
        item->n_classes = 1;
        snprintf(item->label, sizeof(item->label), "%s", g_events[i]);
        item->active = (i == g_selected_event);
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
    if (g_n_events == 0) {
        Elem *item = elem_new("item");
        item->parent = sidebar;
        snprintf(item->label, sizeof(item->label), "(none yet)");
        if (sidebar->n_children < MAX_CHILDREN) sidebar->children[sidebar->n_children++] = item;
    }
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
    apply_css(window, 0);
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
    int sidebar_w = scaled(210);

    if (g_current_tab != COMMON_EVENTS_TAB) {
        /* placeholder tabs: no sidebar/panel geometry needed, drawn as
         * one centered message directly against the window in render_pass() */
        return;
    }

    if (sidebar) {
        apply_css(sidebar, 0);
        if (sidebar->style.has_width && !sidebar->style.width_is_pct) sidebar_w = sidebar->style.width;
        sidebar->x = 0; sidebar->y = content_y; sidebar->w = sidebar_w; sidebar->h = content_h;
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
        panel->x = sidebar_w + margin;
        panel->y = content_y + margin;
        panel->w = window->w - sidebar_w - margin * 2;
        panel->h = content_h - margin * 2;
        int cy = panel->y + scaled(16); /* room for the floating title */
        for (int i = 0; i < panel->n_children; i++) {
            Elem *c = panel->children[i];
            apply_css(c, 0);
            if (strcmp(c->tag, "title") == 0) {
                /* floating block-title: CSS position:absolute + top/left
                 * (default top:-8 left:10) relative to the panel's own box -
                 * painted after the panel's border in render_pass() so the
                 * negative offset visually overlaps it, no special-casing
                 * needed beyond draw order. */
                int t = scaled(c->style.has_top ? c->style.top : -8);
                int l = scaled(c->style.has_left ? c->style.left : 10);
                c->x = panel->x + l;
                c->y = panel->y + t;
                c->w = measure_text_px(&c->style, c->label) + scaled(10);
                c->h = scaled(14);
                continue;
            }
            c->x = panel->x + scaled(12);
            c->y = cy;
            c->w = panel->w - scaled(24);
            c->h = scaled(22);
            cy += c->h + scaled(6);
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
            for (int i = 0; i < panel->n_children && g_n_nav < MAX_ELEMS; i++) {
                Elem *c = panel->children[i];
                if (strcmp(c->tag, "button") != 0) { c->nav_index = 0; continue; }
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
    if (strcmp(e->tag, "item") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#cce5ff"));
        XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
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
    int ok = stbi_write_png("/tmp/db-hq-frame.png", g_frame_w, g_frame_h, 3, g_frame_rgb, g_frame_w * 3);
    fprintf(stderr, ok ? "db-hq: wrote /tmp/db-hq-frame.png (%dx%d)\n" : "db-hq: dump_frame_png: write failed\n", g_frame_w, g_frame_h);
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
    snprintf(title, sizeof(title), "db-hq %s", g_has_real_focus ? "^" : " ");
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

static void open_in_editor(const char *name) {
    char ce_path[PATH_BUF];
    snprintf(ce_path, sizeof(ce_path), "%s/common_events/%s", g_house_root, name);
    char sh[PATH_BUF * 3];
    snprintf(sh, sizeof(sh),
        "setsid nohup sh -c 'sh \"%s/xyzfs/bin/muchi-pet/ops/open_event_ez.sh\" \"%s\" \"%s\"' >/dev/null 2>&1 &",
        g_house_root, ce_path, g_house_root);
    int rc = system(sh);
    (void)rc;
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
    if (strcmp(hit->tag, "item") == 0) {
        for (int i = 0; i < g_n_events; i++) if (strcmp(g_events[i], hit->label) == 0) { g_selected_event = i; break; }
        Elem *sidebar = find_by_tag(g_window, "sidebar");
        inject_sidebar_items(sidebar);
        Elem *panel_text = find_by_tag(g_window, "text");
        if (panel_text && g_selected_event >= 0) snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_events[g_selected_event]);
        redraw();
        return;
    }
    if (strcmp(hit->id, "open-editor") == 0) {
        if (g_selected_event >= 0) open_in_editor(g_events[g_selected_event]);
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
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) {
        if (g_digit_accum > 0) { g_digit_accum = 0; return; }
        g_quit = 1; /* no WM chrome/close button (override_redirect) - Escape closes instead */
        return;
    }
    if (ch >= '0' && ch <= '9') {
        int d = ch - '0';
        int new_val = g_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) {
            g_digit_accum = new_val;
            g_focus_nav = new_val;
            redraw();
        } else if (d > 0 && d <= g_n_nav) {
            g_digit_accum = d;
            g_focus_nav = d;
            redraw();
        } else {
            g_digit_accum = 0;
        }
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
    snprintf(out, outsz, "%s/#.desktop/db_hq_agent_relay.txt", g_house_root);
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
    g_chrome_h = scaled(26);

    memset(g_close_elem, 0, sizeof(*g_close_elem));
    snprintf(g_close_elem->tag, sizeof(g_close_elem->tag), "closebtn");

    load_common_events();
    if (g_n_events > 0) g_selected_event = 0;

    Elem *window = parse_chtpm(argv[2]);
    if (!window) {
        fprintf(stderr, "db-hq: failed to parse %s\n", argv[2]);
        return 1;
    }
    g_window = window;

    Elem *sidebar = find_by_tag(window, "sidebar");
    inject_sidebar_items(sidebar);
    Elem *panel_text = find_by_tag(window, "text");
    if (panel_text && g_selected_event >= 0) snprintf(panel_text->label, sizeof(panel_text->label), "%s", g_events[g_selected_event]);

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

    while (!g_quit) {
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
