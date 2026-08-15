/* khtpm_events_hq_render.c — events-hq, real (not throwaway) event
 * editor. Reads/writes the SAME event_pkg/pages/page_N/event.ir.pdl
 * event-ez itself uses (studied live before writing this - see
 * 2do-rgb-hq.md's "Backend format" section), so both editors stay
 * compatible on the same data.
 *
 * Reuses every proven pattern from tonight's khtpm_hq_render.c (db-hq)
 * and khtpm_rgb_test.c (Phase 0): managed window + _MOTIF_WM_HINTS
 * decorations=0 (NOT override_redirect - see aug-12-END.txt's
 * "RESOLVED" focus section for why that distinction is load-bearing),
 * wraith-alpha-standard index nav, chrome-bar drag, real focus
 * diagnostics ("^" + status line). Layout is hand-computed x/y/w/h
 * (khtpm_css_parser.c has no flex/grid yet - see 2do-rgb-hq.md's own
 * "mockup reality check" for what was deliberately left out of the
 * port).
 *
 * Usage: khtpm_events_hq_render.+x <house_root> <event_pkg_dir> <entity_label>
 */
#include "khtpm_css_parser.h"
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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define PATH_BUF 4096
#define MAX_CHILDREN 32
#define MAX_ELEMS 128
#define MAX_CMDS 64
#define MAX_PAGES 16

typedef struct Elem {
    char tag[32];
    char id[64];
    char classes[CSS_MAX_CLASSES][32];
    int n_classes;
    char label[256];
    int active;
    int nav_index;
    struct Elem *children[MAX_CHILDREN];
    int n_children;
    int x, y, w, h;
    CssStyle style;
} Elem;

static Elem g_pool[MAX_ELEMS];
static int g_n_elems = 0;
static char g_house_root[PATH_BUF];
static char g_pkg_dir[PATH_BUF];
static char g_entity_label[128];

/* Real entity sprite, direct correction 2026-08-12 ("we render from a
 * real png for that character. do u see how it renders its own desktop
 * entity?"): the first attempt (Xft + Noto Color Emoji) hit a real,
 * confirmed-live X RENDER bug (BadLength on RenderAddGlyphs - color-
 * bitmap glyph fonts overflow this X server's RenderAddGlyphs request
 * size) and rendered nothing. The entity's REAL portrait is a raw RGBA
 * pixel dump, sprite.csv, sibling to event_pkg/ - exact same format and
 * parser as 01.muchi-pals-🥚️-13.01/system/egg_window.c's own
 * load_sprite() (studied before writing this, not guessed): a
 * "# resolution=N" header line, then N*N "r,g,b,a" rows in row-major
 * order. Drawn as plain XDrawPoint calls (blended against the toolbar's
 * own solid background color first, since Xlib has no alpha
 * compositing) - small enough (target ~36x36) that per-pixel drawing is
 * fine, no XImage/XPutImage needed. */
static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;

static void load_entity_sprite(void) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/../sprite.csv", g_pkg_dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return; }
    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            pixels[count * 4 + 0] = (unsigned char)r;
            pixels[count * 4 + 1] = (unsigned char)g;
            pixels[count * 4 + 2] = (unsigned char)b;
            pixels[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(pixels); return; }
    g_sprite_pixels = pixels;
    g_sprite_res = res;
}

static Elem *elem_new(const char *tag) {
    if (g_n_elems >= MAX_ELEMS) return NULL;
    Elem *e = &g_pool[g_n_elems++];
    memset(e, 0, sizeof(*e));
    snprintf(e->tag, sizeof(e->tag), "%s", tag);
    return e;
}

/* ---------- tiny generic tag-tree parser (same shape as khtpm_hq_render.c's) ---------- */
static void skip_ws(const char **p) { while (**p && isspace((unsigned char)**p)) (*p)++; }

static void parse_attr_value(const char **p, char *out, size_t outsz) {
    skip_ws(p);
    if (**p != '"') { out[0] = '\0'; return; }
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') { if (n + 1 < outsz) out[n++] = **p; (*p)++; }
    out[n] = '\0';
    if (**p == '"') (*p)++;
}

static void apply_attr(Elem *e, const char *name, const char *val) {
    if (strcmp(name, "id") == 0) snprintf(e->id, sizeof(e->id), "%s", val);
    else if (strcmp(name, "class") == 0) {
        char tmp[128]; snprintf(tmp, sizeof(tmp), "%s", val);
        char *tok = strtok(tmp, " ");
        while (tok && e->n_classes < CSS_MAX_CLASSES) {
            snprintf(e->classes[e->n_classes], sizeof(e->classes[0]), "%s", tok);
            e->n_classes++;
            tok = strtok(NULL, " ");
        }
    } else if (strcmp(name, "label") == 0) snprintf(e->label, sizeof(e->label), "%s", val);
}

static const char *parse_element(const char *p, Elem *parent) {
    skip_ws(&p);
    if (*p != '<') return p;
    p++;
    if (*p == '!') { const char *end = strstr(p, "-->"); return end ? end + 3 : p + strlen(p); }
    char tag[32]; size_t tn = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/') { if (tn + 1 < sizeof(tag)) tag[tn++] = *p; p++; }
    tag[tn] = '\0';
    Elem *e = elem_new(tag);
    if (parent && parent->n_children < MAX_CHILDREN) parent->children[parent->n_children++] = e;
    for (;;) {
        skip_ws(&p);
        if (*p == '/' && p[1] == '>') { p += 2; return p; }
        if (*p == '>') { p++; break; }
        if (!*p) return p;
        char attr[32]; size_t an = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && *p != '>' && *p != '/') { if (an + 1 < sizeof(attr)) attr[an++] = *p; p++; }
        attr[an] = '\0';
        skip_ws(&p);
        char val[256] = "";
        if (*p == '=') { p++; parse_attr_value(&p, val, sizeof(val)); }
        if (attr[0]) apply_attr(e, attr, val);
    }
    for (;;) {
        skip_ws(&p);
        if (!*p) return p;
        if (p[0] == '<' && p[1] == '/') { const char *end = strchr(p, '>'); return end ? end + 1 : p + strlen(p); }
        p = parse_element(p, e);
    }
}

static Elem *parse_chtpm(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = '\0';
    fclose(f);
    const char *p = buf;
    Elem *root = elem_new("__root__");
    for (;;) {
        skip_ws(&p);
        if (!*p) break;
        if (p[0] == '<' && p[1] == '!') { const char *end = strstr(p, "-->"); p = end ? end + 3 : p + strlen(p); continue; }
        break;
    }
    if (*p == '<') parse_element(p, root);
    if (root->n_children > 0) root = root->children[0];
    free(buf);
    return root;
}

static Elem *find_by_id(Elem *e, const char *id) {
    if (!e) return NULL;
    if (strcmp(e->id, id) == 0) return e;
    for (int i = 0; i < e->n_children; i++) { Elem *r = find_by_id(e->children[i], id); if (r) return r; }
    return NULL;
}

/* ---------- real backend data: pages + commands, event_pkg/pages/page_N/ ---------- */

typedef struct {
    int id;
    char type[32];   /* change_gold / show_text / show_choices */
    char params[512]; /* raw text after "| " on the NODE row, for display */
} CmdNode;

static char g_pages[MAX_PAGES][64];
static int g_n_pages = 0;
static int g_current_page = 0; /* 0-based index into g_pages */

static CmdNode g_cmds[MAX_CMDS];
static int g_n_cmds = 0;
static char g_trigger[64] = "(unknown)";

static void page_dir(char *out, size_t outsz, int page_idx) {
    snprintf(out, outsz, "%s/pages/%s", g_pkg_dir, g_pages[page_idx]);
}

static void load_pages(void) {
    g_n_pages = 0;
    char pages_root[PATH_BUF];
    snprintf(pages_root, sizeof(pages_root), "%s/pages", g_pkg_dir);
    if (access(pages_root, F_OK) != 0) mkdir(pages_root, 0755);
    DIR *d = opendir(pages_root);
    if (d) {
        struct dirent *de;
        char names[MAX_PAGES][64];
        int n = 0;
        while ((de = readdir(d)) && n < MAX_PAGES) {
            if (de->d_name[0] == '.') continue;
            if (strncmp(de->d_name, "page_", 5) != 0) continue;
            snprintf(names[n], sizeof(names[0]), "%s", de->d_name);
            n++;
        }
        closedir(d);
        for (int i = 0; i < n - 1; i++)
            for (int j = i + 1; j < n; j++)
                if (atoi(names[j] + 5) < atoi(names[i] + 5)) {
                    char t[64]; snprintf(t, sizeof(t), "%s", names[i]);
                    snprintf(names[i], sizeof(names[i]), "%s", names[j]);
                    snprintf(names[j], sizeof(names[j]), "%s", t);
                }
        for (int i = 0; i < n; i++) snprintf(g_pages[i], sizeof(g_pages[0]), "%s", names[i]);
        g_n_pages = n;
    }
    if (g_n_pages == 0) {
        /* real, not a stub: create page_1 fresh, matching event-ez's own
         * lazy-scaffold-on-first-use behavior (ez_menu_input.c creates
         * pages/page_N lazily too, not the launcher). */
        char p1[PATH_BUF]; snprintf(p1, sizeof(p1), "%s/page_1", pages_root);
        mkdir(p1, 0755);
        snprintf(g_pages[0], sizeof(g_pages[0]), "page_1");
        g_n_pages = 1;
    }
    if (g_current_page >= g_n_pages) g_current_page = 0;
}

static void load_condition(void) {
    snprintf(g_trigger, sizeof(g_trigger), "(unset)");
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
    char cpath[PATH_BUF]; snprintf(cpath, sizeof(cpath), "%s/condition.pdl", pd);
    FILE *f = fopen(cpath, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COND", 4) != 0) continue;
        char *pipe1 = strchr(line, '|');
        if (!pipe1) continue;
        char *pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) continue;
        char val[64]; snprintf(val, sizeof(val), "%s", pipe2 + 1);
        char *nl = strpbrk(val, "\r\n"); if (nl) *nl = '\0';
        char *s = val; while (*s == ' ') s++;
        snprintf(g_trigger, sizeof(g_trigger), "%s", s);
        break;
    }
    fclose(f);
}

/* parses event.ir.pdl's own "NODE | id=N type=<t> | <params>" rows -
 * same format event-ez itself writes/reads (ez_menu_input.c), not a
 * new format. */
static void load_commands(void) {
    g_n_cmds = 0;
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);
    FILE *f = fopen(ir_path, "r");
    if (!f) return;
    char line[MAX_ELEMS < 512 ? 512 : 512];
    while (fgets(line, sizeof(line), f) && g_n_cmds < MAX_CMDS) {
        if (strncmp(line, "NODE", 4) != 0) continue;
        char *tp = strstr(line, "type=");
        if (!tp) continue;
        char *t = tp + 5;
        char *sp = strchr(t, ' ');
        char *pipe = strchr(t, '|');
        size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
        if (len >= sizeof(g_cmds[0].type)) len = sizeof(g_cmds[0].type) - 1;
        memcpy(g_cmds[g_n_cmds].type, t, len);
        g_cmds[g_n_cmds].type[len] = '\0';
        char *idp = strstr(line, "id=");
        g_cmds[g_n_cmds].id = idp ? atoi(idp + 3) : (g_n_cmds + 1);
        char *pipe2 = pipe ? strchr(pipe + 1, '|') : NULL;
        const char *params_start = pipe2 ? pipe2 + 1 : "";
        while (*params_start == ' ') params_start++;
        snprintf(g_cmds[g_n_cmds].params, sizeof(g_cmds[0].params), "%s", params_start);
        char *nl = strpbrk(g_cmds[g_n_cmds].params, "\r\n");
        if (nl) *nl = '\0';
        g_n_cmds++;
    }
    fclose(f);
}

/* Real compile pass, ported line-for-line from ez_menu_input.c's own
 * (change_gold/show_text/show_choices compile blocks) - event.pal is
 * ALWAYS fully regenerated from event.ir.pdl, never hand-patched, same
 * "visual compiler" semantics event-ez itself uses, so output stays
 * byte-compatible with what event-ez would produce for the same IR. */
static void compile_page(int page_idx) {
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), page_idx);
    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);
    char pal_path[PATH_BUF]; snprintf(pal_path, sizeof(pal_path), "%s/event.pal", pd);

    FILE *pf = fopen(pal_path, "w");
    if (!pf) return;
    fprintf(pf, "# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl by events-hq\n");
    fprintf(pf, "# pkg=%s page=%s - regenerated fresh on every command save\n", g_entity_label, g_pages[page_idx]);
    FILE *irf = fopen(ir_path, "r");
    if (irf) {
        char line[512];
        while (fgets(line, sizeof(line), irf)) {
            if (strncmp(line, "NODE", 4) != 0) continue;
            char *tp = strstr(line, "type=");
            if (!tp) continue;
            char type_buf[48] = "";
            char *t = tp + 5, *sp = strchr(t, ' ');
            char *pipe = strchr(t, '|');
            size_t len = sp ? (size_t)(sp - t) : (pipe ? (size_t)(pipe - t) : strlen(t));
            if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
            memcpy(type_buf, t, len); type_buf[len] = '\0';
            char *idp = strstr(line, "id=");
            int node_id = idp ? atoi(idp + 3) : 1;
            char wrapper_path[PATH_BUF];
            snprintf(wrapper_path, sizeof(wrapper_path), "%s/cmd_%d.sh", pd, node_id);
            FILE *wf = fopen(wrapper_path, "w");
            if (wf) {
                fprintf(wf, "#!/bin/sh\n");
                fprintf(wf, "cd \"$(dirname \"$0\")/../../..\" || exit 1\n");
                fprintf(wf, "ENT=\"$PWD\"\n");
                fprintf(wf, "D=\"$ENT\"\n");
                fprintf(wf, "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n");
                if (strcmp(type_buf, "change_gold") == 0) {
                    char *amtp = strstr(line, "amount=");
                    char amt[32] = "0";
                    if (amtp) { snprintf(amt, sizeof(amt), "%s", amtp + 7); amt[strcspn(amt, "\r\n| ")] = '\0'; }
                    fprintf(wf, "exec \"$D/xyzfs/bin/muchi-pet/ops/+x/mr_change_gold.+x\" \"$ENT\" '%s'\n", amt);
                } else if (strcmp(type_buf, "show_text") == 0) {
                    char *txtp = strstr(line, "text=");
                    char txt[256] = "";
                    if (txtp) { snprintf(txt, sizeof(txt), "%s", txtp + 5); char *bar = strchr(txt, '|'); if (bar) *bar = '\0'; txt[strcspn(txt, "\r\n")] = '\0'; }
                    char *spkp = strstr(line, "speaker=");
                    char spk[64] = "";
                    if (spkp) { snprintf(spk, sizeof(spk), "%s", spkp + 8); spk[strcspn(spk, "\r\n")] = '\0'; }
                    if (spk[0]) fprintf(wf, "exec \"$D/xyzfs/bin/muchi-pet/ops/+x/mr_show_text.+x\" \"$ENT\" '%s' '%s'\n", txt, spk);
                    else fprintf(wf, "exec \"$D/xyzfs/bin/muchi-pet/ops/+x/mr_show_text.+x\" \"$ENT\" '%s'\n", txt);
                } else if (strcmp(type_buf, "show_choices") == 0) {
                    char *chp = strstr(line, "choices=");
                    char ch[256] = "";
                    if (chp) { snprintf(ch, sizeof(ch), "%s", chp + 8); char *d = strstr(ch, " default="); if (d) *d = '\0'; ch[strcspn(ch, "\r\n")] = '\0'; }
                    char *defp = strstr(line, "default=");
                    int def = defp ? atoi(defp + 8) : 0;
                    fprintf(wf, "exec \"$D/xyzfs/bin/muchi-pet/ops/+x/mr_show_choices.+x\" \"$ENT\" '%s' %d\n", ch, def);
                }
                fclose(wf);
                chmod(wrapper_path, 0755);
            }
            fprintf(pf, "exec cmd_%d.sh\n", node_id);
        }
        fclose(irf);
    }
    fprintf(pf, "halt\n");
    fclose(pf);
}

static void append_node_and_compile(const char *type, const char *params_line) {
    char pd[PATH_BUF]; page_dir(pd, sizeof(pd), g_current_page);
    char ir_path[PATH_BUF]; snprintf(ir_path, sizeof(ir_path), "%s/event.ir.pdl", pd);

    int next_id = 1;
    FILE *rf = fopen(ir_path, "r");
    if (rf) {
        char line[512];
        while (fgets(line, sizeof(line), rf)) if (strncmp(line, "NODE", 4) == 0) next_id++;
        fclose(rf);
    } else {
        /* fresh page - real header, matching ez_menu_input.c's own
         * "a fresh page needs a real header + a terminating NODE"
         * convention. */
        FILE *hf = fopen(ir_path, "w");
        if (hf) {
            fprintf(hf, "SECTION      | KEY                | VALUE\n");
            fprintf(hf, "----------------------------------------\n");
            fprintf(hf, "META         | piece_id           | %s\n", g_entity_label);
            fprintf(hf, "STATE        | source               | events-hq\n");
            fclose(hf);
        }
    }
    FILE *af = fopen(ir_path, "a");
    if (!af) return;
    fprintf(af, "NODE         | id=%d type=%s | %s\n", next_id, type, params_line);
    fclose(af);

    compile_page(g_current_page);
    load_commands();
}

/* ---------- X11/Xft globals, scale, layout (fixed x/y/w/h - no flex/grid) ---------- */
static Display *dpy;
static int screen;
static Window win;
static GC gc;
static Pixmap buf;
static XftDraw *xftdraw_buf;
static Colormap cmap;
static Elem *g_window;
static const CssSheet *g_sheet;
static int g_quit = 0;
static int g_has_real_focus = 0;
static char g_last_key_label[32] = "";

static int g_dragging = 0;
static int g_drag_last_x = 0, g_drag_last_y = 0;
static int g_win_x = 120, g_win_y = 120;

static int g_toolbar_y = 0, g_toolbar_h = 0; /* for draw_entity_glyph()'s own positioning */
static Elem g_close_elem_storage;
static Elem *g_close_elem = &g_close_elem_storage;
static int g_close_x, g_close_y, g_close_w, g_close_h;
#define CHROME_H 26

static Elem *g_nav[MAX_ELEMS];
static int g_n_nav = 0;
static int g_focus_nav = 1;
static int g_digit_accum = 0;

/* picker (add-command) mode state - own simple modal-like overlay,
 * drawn directly (no separate popout process - simpler state sharing
 * for a small 3-type form), matching 2do-rgb-hq.md's own scope call
 * ("command-type picker... a small real popout" - implemented here as
 * an in-window overlay instead of a second X11 window for simplicity,
 * a real, working substitution for the mockup's own position:fixed
 * modal, which the CSS engine can't do yet either). */
static int g_picker_open = 0;
static int g_picker_type = -1; /* 0=change_gold 1=show_text 2=show_choices, -1=choosing */
static int g_picker_focus = 1; /* own nav counter for the 3 type choices - separate from g_focus_nav, which belongs to the outer window */
static char g_field1[256] = "", g_field2[256] = "";
static int g_active_field = 0; /* 0 or 1 */
static const char *PICKER_TYPES[] = { "change_gold", "show_text", "show_choices" };
static const char *PICKER_LABELS[] = { "Change Gold", "Show Text", "Show Choices" };

static void apply_css(Elem *e) {
    css_compute_style(g_sheet, e->tag, e->id[0] ? e->id : NULL, e->classes, e->n_classes, 0, &e->style);
}

static int measure_text_px(const CssStyle *st, const char *text) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = st->has_font_size ? st->font_size : 11;
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    if (!f) f = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=11");
    if (!f) return (int)strlen(text) * 7;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, f, (const FcChar8 *)text, (int)strlen(text), &ext);
    XftFontClose(dpy, f);
    return ext.width;
}

static void layout_pass(Elem *window) {
    apply_css(window);
    window->x = 0; window->y = 0;
    window->w = 720; window->h = 480;

    g_close_w = 56; g_close_h = CHROME_H - 6;
    g_close_x = window->w - g_close_w - 4;
    g_close_y = 3;

    Elem *toolbar = find_by_id(window, "toolbar");
    Elem *pagetabs = find_by_id(window, "pagetabs");
    Elem *left = find_by_id(window, "left");
    Elem *right = find_by_id(window, "right");
    Elem *footer = find_by_id(window, "footer");

    int toolbar_h = 46, tabs_h = 26, footer_h = 34; /* taller toolbar to fit the real entity glyph, see draw_entity_glyph() */
    int y = CHROME_H;
    if (toolbar) {
        apply_css(toolbar);
        toolbar->x = 0; toolbar->y = y; toolbar->w = window->w; toolbar->h = toolbar_h;
        g_toolbar_y = toolbar->y; g_toolbar_h = toolbar->h;
        for (int i = 0; i < toolbar->n_children; i++) {
            Elem *c = toolbar->children[i]; apply_css(c);
            /* +46 leaves room for the glyph icon drawn separately in
             * draw_entity_glyph() (chrome, not a tag-tree element - same
             * approach the close button already uses). */
            c->x = 46; c->y = toolbar->y + toolbar_h / 2 - 9; c->w = window->w - 56; c->h = 18;
        }
        y += toolbar_h;
    }
    if (pagetabs) {
        apply_css(pagetabs);
        pagetabs->x = 0; pagetabs->y = y; pagetabs->w = window->w; pagetabs->h = tabs_h;
        int tx = 4;
        for (int i = 0; i < pagetabs->n_children; i++) {
            Elem *tab = pagetabs->children[i]; apply_css(tab);
            int tw = measure_text_px(&tab->style, tab->label) + 30;
            tab->x = tx; tab->y = y + 2; tab->w = tw; tab->h = tabs_h - 4;
            tx += tw + 1;
        }
        y += tabs_h;
    }
    int content_y = y, content_h = window->h - y - footer_h;
    int left_w = 220;
    if (left) {
        apply_css(left);
        left->x = 4; left->y = content_y + 8; left->w = left_w; left->h = content_h - 12;
        int cy = left->y + 16;
        for (int i = 0; i < left->n_children; i++) {
            Elem *c = left->children[i]; apply_css(c);
            if (strcmp(c->tag, "title") == 0) {
                c->x = left->x + 10; c->y = left->y - 8; c->w = measure_text_px(&c->style, c->label) + 10; c->h = 14;
                continue;
            }
            c->x = left->x + 10; c->y = cy; c->w = left->w - 20; c->h = 18;
            cy += 24;
        }
    }
    if (right) {
        apply_css(right);
        right->x = left_w + 8; right->y = content_y + 8; right->w = window->w - left_w - 16; right->h = content_h - 12;
        int cy = right->y + 20;
        for (int i = 0; i < right->n_children; i++) {
            Elem *c = right->children[i]; apply_css(c);
            if (strcmp(c->tag, "title") == 0) {
                c->x = right->x + 10; c->y = right->y - 8; c->w = measure_text_px(&c->style, c->label) + 10; c->h = 14;
                continue;
            }
            c->x = right->x + 12; c->y = cy; c->w = right->w - 24; c->h = 18;
            cy += 22;
        }
    }
    if (footer) {
        apply_css(footer);
        footer->x = 0; footer->y = window->h - footer_h; footer->w = window->w; footer->h = footer_h;
        int fx = 10;
        for (int i = 0; i < footer->n_children; i++) {
            Elem *c = footer->children[i]; apply_css(c);
            int cw = measure_text_px(&c->style, c->label) + 20;
            c->x = fx; c->y = footer->y + 6; c->w = cw; c->h = footer_h - 12;
            fx += cw + 8;
        }
    }
}

/* rebuild the command-list panel's children from g_cmds (real data),
 * replacing the placeholder <text id="cmd-empty"> - same
 * inject-at-runtime pattern db-hq's own inject_sidebar_items() used. */
static void inject_commands(Elem *window) {
    Elem *right = find_by_id(window, "right");
    if (!right) return;
    /* keep the <title> child, drop everything else, rebuild */
    Elem *title = NULL;
    for (int i = 0; i < right->n_children; i++) if (strcmp(right->children[i]->tag, "title") == 0) title = right->children[i];
    right->n_children = 0;
    if (title) right->children[right->n_children++] = title;
    if (g_n_cmds == 0) {
        Elem *e = elem_new("text");
        snprintf(e->classes[0], sizeof(e->classes[0]), "empty-msg"); e->n_classes = 1;
        snprintf(e->label, sizeof(e->label), "(no commands yet)");
        right->children[right->n_children++] = e;
        return;
    }
    for (int i = 0; i < g_n_cmds && right->n_children < MAX_CHILDREN; i++) {
        Elem *e = elem_new("text");
        char cls[48]; snprintf(cls, sizeof(cls), "cmd-%s", g_cmds[i].type);
        snprintf(e->classes[0], sizeof(e->classes[0]), "%s", cls); e->n_classes = 1;
        snprintf(e->label, sizeof(e->label), "%d. %s  %s", g_cmds[i].id, g_cmds[i].type, g_cmds[i].params);
        right->children[right->n_children++] = e;
    }
}

static void refresh_page_data(Elem *window) {
    load_condition();
    load_commands();
    Elem *tv = find_by_id(window, "trigger-value");
    if (tv) snprintf(tv->label, sizeof(tv->label), "%s", g_trigger);
    inject_commands(window);
    Elem *pagetabs = find_by_id(window, "pagetabs");
    if (pagetabs) {
        pagetabs->n_children = 0;
        for (int i = 0; i < g_n_pages && pagetabs->n_children < MAX_CHILDREN; i++) {
            Elem *t = elem_new("tab");
            snprintf(t->label, sizeof(t->label), "%s", g_pages[i]);
            t->active = (i == g_current_page);
            pagetabs->children[pagetabs->n_children++] = t;
        }
    }
    Elem *en = find_by_id(window, "event-name");
    if (en) snprintf(en->label, sizeof(en->label), "%s", g_entity_label);
}

static void assign_nav_indices(Elem *window) {
    g_n_nav = 0;
    Elem *pagetabs = find_by_id(window, "pagetabs");
    if (pagetabs) for (int i = 0; i < pagetabs->n_children && g_n_nav < MAX_ELEMS; i++) {
        pagetabs->children[i]->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = pagetabs->children[i];
    }
    Elem *footer = find_by_id(window, "footer");
    if (footer) for (int i = 0; i < footer->n_children && g_n_nav < MAX_ELEMS; i++) {
        footer->children[i]->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = footer->children[i];
    }
    if (g_n_nav < MAX_ELEMS) { g_close_elem->nav_index = ++g_n_nav; g_nav[g_n_nav - 1] = g_close_elem; }
    if (g_focus_nav < 1) g_focus_nav = 1;
    if (g_focus_nav > g_n_nav) g_focus_nav = g_n_nav > 0 ? g_n_nav : 1;
}

/* ---------- rendering ---------- */
static unsigned long alloc_pixel(const char *spec) {
    if (!spec || !spec[0]) return BlackPixel(dpy, screen);
    XColor c;
    if (spec[0] == '#') { if (XParseColor(dpy, cmap, spec, &c) && XAllocColor(dpy, cmap, &c)) return c.pixel; }
    else if (XAllocNamedColor(dpy, cmap, spec, &c, &c)) return c.pixel;
    return BlackPixel(dpy, screen);
}

static XftColor xft_color(const char *spec) {
    XftColor xc; XRenderColor rc = {0, 0, 0, 0xffff};
    if (spec && spec[0] == '#' && strlen(spec) >= 7) {
        unsigned int r, g, b; sscanf(spec + 1, "%02x%02x%02x", &r, &g, &b);
        rc.red = (unsigned short)(r * 257); rc.green = (unsigned short)(g * 257); rc.blue = (unsigned short)(b * 257);
    }
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), cmap, &rc, &xc); return xc;
}

static XftFont *font_for(const CssStyle *st) {
    char spec[128];
    const char *fam = st->has_font_family ? st->font_family : "DejaVu Sans";
    int size = st->has_font_size ? st->font_size : 11;
    snprintf(spec, sizeof(spec), "%s:pixelsize=%d%s", fam, size, (st->has_font_weight && st->font_weight_bold) ? ":bold" : "");
    XftFont *f = XftFontOpenName(dpy, screen, spec);
    return f ? f : XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=11");
}

static void draw_elem(Elem *e) {
    if (e->style.has_bg_color) { XSetForeground(dpy, gc, alloc_pixel(e->style.bg_color)); XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h); }
    if (e->style.has_border_color) {
        XSetForeground(dpy, gc, alloc_pixel(e->style.border_color));
        XDrawRectangle(dpy, buf, gc, e->x, e->y, e->w - 1, e->h - 1);
    }
    if (strcmp(e->tag, "tab") == 0 && e->active && !e->style.has_bg_color) {
        XSetForeground(dpy, gc, alloc_pixel("#3a3a3a")); XFillRectangle(dpy, buf, gc, e->x, e->y, e->w, e->h);
    }
    if (e->nav_index > 0 && e->nav_index == g_focus_nav) {
        XSetForeground(dpy, gc, alloc_pixel("#ff8c00"));
        XDrawRectangle(dpy, buf, gc, e->x - 1, e->y - 1, e->w + 1, e->h + 1);
    }
    int pad = 4;
    int label_x = e->x + pad;
    if (e->nav_index > 0) {
        char badge[16];
        int focused = (e->nav_index == g_focus_nav);
        /* REAL FIX 2026-08-12 - see khtpm_hq_render.c's identical fix
         * for the full citation: real std is `[state]N.`, not `[stateN]`. */
        snprintf(badge, sizeof(badge), "[%c]%d.", focused ? '>' : ' ', e->nav_index);
        char numspec[48]; snprintf(numspec, sizeof(numspec), "DejaVu Sans Mono:pixelsize=9");
        XftFont *numfont = XftFontOpenName(dpy, screen, numspec);
        if (numfont) {
            XftColor numcol = xft_color(focused ? "#ff8c00" : "#9a9a9a");
            XGlyphInfo numext; XftTextExtentsUtf8(dpy, numfont, (const FcChar8 *)badge, (int)strlen(badge), &numext);
            int numy = e->y + (e->h + numfont->ascent - numfont->descent) / 2;
            XftDrawStringUtf8(xftdraw_buf, &numcol, numfont, label_x, numy, (const FcChar8 *)badge, (int)strlen(badge));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &numcol);
            label_x += numext.width + 5;
            XftFontClose(dpy, numfont);
        }
    }
    if (e->label[0]) {
        XftFont *font = font_for(&e->style);
        if (font) {
            XftColor col = xft_color(e->style.has_fg_color ? e->style.fg_color : "#cccccc");
            int ty = e->y + (e->h + font->ascent - font->descent) / 2;
            XftDrawStringUtf8(xftdraw_buf, &col, font, label_x, ty, (const FcChar8 *)e->label, (int)strlen(e->label));
            XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &col);
            XftFontClose(dpy, font);
        }
    }
}

static void render_tree(Elem *e) {
    for (int i = 0; i < e->n_children; i++) {
        Elem *c = e->children[i];
        if (strcmp(c->tag, "title") == 0) continue;
        draw_elem(c);
        render_tree(c);
    }
    for (int i = 0; i < e->n_children; i++) if (strcmp(e->children[i]->tag, "title") == 0) draw_elem(e->children[i]);
}

/* real entity portrait/glyph, drawn large in the toolbar (not a
 * tag-tree Elem - chrome, same approach the close button already
 * uses). Direct request: "make sure we get the image of the character
 * in that window early on also for user sanity" - so it's the FIRST
 * real thing visible under the title bar, confirming which entity is
 * actually being edited before touching anything. Needs Noto Color
 * Emoji specifically (confirmed installed: `fc-list | grep -i emoji`)
 * - DejaVu Sans has no emoji glyphs. */
static void draw_entity_glyph(void) {
    if (!g_sprite_pixels || g_sprite_res <= 0) return;
    int size = 36; /* target on-screen box */
    int ox = 6, oy = g_toolbar_y + (g_toolbar_h - size) / 2;
    /* toolbar's own solid bg color (#2f2f2f, dashboard.css's .toolbar) -
     * blended against manually since Xlib point-drawing has no alpha
     * compositing of its own. */
    int bg_r = 0x2f, bg_g = 0x2f, bg_b = 0x2f;
    for (int y = 0; y < size; y++) {
        int sy = y * g_sprite_res / size;
        for (int x = 0; x < size; x++) {
            int sx = x * g_sprite_res / size;
            const unsigned char *px = &g_sprite_pixels[(sy * g_sprite_res + sx) * 4];
            int a = px[3];
            if (a == 0) continue;
            int r = (px[0] * a + bg_r * (255 - a)) / 255;
            int g = (px[1] * a + bg_g * (255 - a)) / 255;
            int b = (px[2] * a + bg_b * (255 - a)) / 255;
            char spec[8]; snprintf(spec, sizeof(spec), "#%02x%02x%02x", r, g, b);
            XSetForeground(dpy, gc, alloc_pixel(spec));
            XDrawPoint(dpy, buf, gc, ox + x, oy + y);
        }
    }
}

static void draw_chrome_bar(void) {
    XSetForeground(dpy, gc, alloc_pixel("#1c1c1c"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, CHROME_H);
    char tspec[48]; snprintf(tspec, sizeof(tspec), "DejaVu Sans:pixelsize=10:bold");
    XftFont *titlefont = XftFontOpenName(dpy, screen, tspec);
    if (titlefont) {
        XftColor titlecol = xft_color("#eeeeee");
        char title[48]; snprintf(title, sizeof(title), "events-hq %s", g_has_real_focus ? "^" : " ");
        int ty = (CHROME_H + titlefont->ascent - titlefont->descent) / 2;
        XftDrawStringUtf8(xftdraw_buf, &titlecol, titlefont, 8, ty, (const FcChar8 *)title, (int)strlen(title));
        XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &titlecol);
        XftFontClose(dpy, titlefont);
    }
    g_close_elem->x = g_close_x; g_close_elem->y = g_close_y; g_close_elem->w = g_close_w; g_close_elem->h = g_close_h;
    snprintf(g_close_elem->label, sizeof(g_close_elem->label), "x");
    css_style_init(&g_close_elem->style);
    g_close_elem->style.has_border_color = 1;
    snprintf(g_close_elem->style.border_color, sizeof(g_close_elem->style.border_color), "%s", g_close_elem->nav_index == g_focus_nav ? "#ff8c00" : "#888888");
    g_close_elem->style.has_fg_color = 1;
    snprintf(g_close_elem->style.fg_color, sizeof(g_close_elem->style.fg_color), "#eeeeee");
    draw_elem(g_close_elem);
}

static void draw_picker_overlay(void) {
    int pw = 360, ph = 160;
    int px = (g_window->w - pw) / 2, py = (g_window->h - ph) / 2;
    XSetForeground(dpy, gc, alloc_pixel("#2a2a2a"));
    XFillRectangle(dpy, buf, gc, px, py, pw, ph);
    XSetForeground(dpy, gc, alloc_pixel("#4a9eff"));
    XDrawRectangle(dpy, buf, gc, px, py, pw - 1, ph - 1);
    XftFont *font = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=11");
    XftFont *bfont = XftFontOpenName(dpy, screen, "DejaVu Sans:pixelsize=11:bold");
    if (!font || !bfont) return;
    XftColor white = xft_color("#eeeeee");
    XftColor gray = xft_color("#999999");
    int ty = py + 20;
    if (g_picker_type < 0) {
        const char *hdr = "Add Command";
        XftDrawStringUtf8(xftdraw_buf, &white, bfont, px + 14, ty, (const FcChar8 *)hdr, (int)strlen(hdr));
        ty += 26;
        /* real wraith-alpha nav in the picker too, not a bespoke "press
         * 1/2/3" scheme - direct correction 2026-08-12: "submenus should
         * use nav as well. u stopped using nav for add command? no
         * good". Same "[>N]"/"[ N]" bracket-badge convention as
         * everywhere else in the app - g_picker_focus is its own
         * counter (not g_focus_nav, which belongs to the outer window
         * and must stay untouched while this overlay is open). */
        for (int i = 0; i < 3; i++) {
            int focused = (i + 1 == g_picker_focus);
            /* REAL FIX 2026-08-12 - real std is `[state]N.`, see
             * khtpm_hq_render.c's identical fix for the full citation. */
            char badge[16]; snprintf(badge, sizeof(badge), "[%c]%d.", focused ? '>' : ' ', i + 1);
            XftColor *bc = focused ? &white : &gray;
            XftDrawStringUtf8(xftdraw_buf, bc, font, px + 20, ty, (const FcChar8 *)badge, (int)strlen(badge));
            char line[64]; snprintf(line, sizeof(line), "%s", PICKER_LABELS[i]);
            XftDrawStringUtf8(xftdraw_buf, &white, font, px + 60, ty, (const FcChar8 *)line, (int)strlen(line));
            if (focused) { XSetForeground(dpy, gc, alloc_pixel("#ff8c00")); XDrawRectangle(dpy, buf, gc, px + 16, ty - 14, pw - 32, 18); }
            ty += 22;
        }
        const char *hint = "Digits/arrows + Enter select, Escape cancels";
        XftDrawStringUtf8(xftdraw_buf, &gray, font, px + 14, py + ph - 14, (const FcChar8 *)hint, (int)strlen(hint));
    } else {
        char hdr[64]; snprintf(hdr, sizeof(hdr), "%s", PICKER_LABELS[g_picker_type]);
        XftDrawStringUtf8(xftdraw_buf, &white, bfont, px + 14, ty, (const FcChar8 *)hdr, (int)strlen(hdr));
        ty += 30;
        const char *f1_label = strcmp(PICKER_TYPES[g_picker_type], "change_gold") == 0 ? "Amount:" :
                                strcmp(PICKER_TYPES[g_picker_type], "show_text") == 0 ? "Text:" : "Choices (comma-sep):";
        char f1line[300]; snprintf(f1line, sizeof(f1line), "%s %s%s", f1_label, g_field1, g_active_field == 0 ? "_" : "");
        XftColor *c1 = g_active_field == 0 ? &white : &gray;
        XftDrawStringUtf8(xftdraw_buf, c1, font, px + 20, ty, (const FcChar8 *)f1line, (int)strlen(f1line));
        ty += 24;
        if (strcmp(PICKER_TYPES[g_picker_type], "change_gold") != 0) {
            const char *f2_label = strcmp(PICKER_TYPES[g_picker_type], "show_text") == 0 ? "Speaker (opt):" : "Default index:";
            char f2line[300]; snprintf(f2line, sizeof(f2line), "%s %s%s", f2_label, g_field2, g_active_field == 1 ? "_" : "");
            XftColor *c2 = g_active_field == 1 ? &white : &gray;
            XftDrawStringUtf8(xftdraw_buf, c2, font, px + 20, ty, (const FcChar8 *)f2line, (int)strlen(f2line));
            ty += 24;
        }
        const char *hint2 = "Enter: next/submit  Escape: cancel";
        XftDrawStringUtf8(xftdraw_buf, &gray, font, px + 14, py + ph - 14, (const FcChar8 *)hint2, (int)strlen(hint2));
    }
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &white);
    XftColorFree(dpy, DefaultVisual(dpy, screen), cmap, &gray);
    XftFontClose(dpy, font); XftFontClose(dpy, bfont);
}

static void redraw(void) {
    layout_pass(g_window);
    assign_nav_indices(g_window);
    XSetForeground(dpy, gc, alloc_pixel("#252525"));
    XFillRectangle(dpy, buf, gc, 0, 0, g_window->w, g_window->h);
    render_tree(g_window);
    draw_entity_glyph();
    draw_chrome_bar();
    if (g_picker_open) draw_picker_overlay();
    XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)g_window->w, (unsigned)g_window->h, 0, 0);
    XFlush(dpy);
}

/* ---------- input ---------- */
static Elem *hit_test(Elem *e, int px, int py) {
    for (int i = e->n_children - 1; i >= 0; i--) { Elem *r = hit_test(e->children[i], px, py); if (r) return r; }
    if (px >= e->x && px < e->x + e->w && py >= e->y && py < e->y + e->h) return e;
    return NULL;
}

static void activate_elem(Elem *hit) {
    if (!hit) return;
    if (strcmp(hit->tag, "closebtn") == 0) { g_quit = 1; return; }
    if (strcmp(hit->tag, "tab") == 0) {
        for (int i = 0; i < g_n_pages; i++) if (strcmp(hit->label, g_pages[i]) == 0) { g_current_page = i; break; }
        refresh_page_data(g_window);
        redraw();
        return;
    }
    if (strcmp(hit->id, "add-command") == 0) {
        g_picker_open = 1; g_picker_type = -1; g_picker_focus = 1; g_field1[0] = '\0'; g_field2[0] = '\0'; g_active_field = 0;
        redraw();
        return;
    }
}

static void handle_click(int px, int py) {
    if (px >= g_close_elem->x && px < g_close_elem->x + g_close_elem->w && py >= g_close_elem->y && py < g_close_elem->y + g_close_elem->h) {
        g_focus_nav = g_close_elem->nav_index; activate_elem(g_close_elem); return;
    }
    Elem *hit = hit_test(g_window, px, py);
    if (!hit) return;
    if (hit->nav_index > 0) g_focus_nav = hit->nav_index;
    activate_elem(hit);
}

static void submit_picker(void) {
    const char *type = PICKER_TYPES[g_picker_type];
    char params[512];
    if (strcmp(type, "change_gold") == 0) snprintf(params, sizeof(params), "amount=%s", g_field1[0] ? g_field1 : "0");
    else if (strcmp(type, "show_text") == 0) {
        if (g_field2[0]) snprintf(params, sizeof(params), "text=%s speaker=%s", g_field1, g_field2);
        else snprintf(params, sizeof(params), "text=%s", g_field1);
    } else snprintf(params, sizeof(params), "choices=%s default=%s", g_field1, g_field2[0] ? g_field2 : "0");
    append_node_and_compile(type, params);
    inject_commands(g_window);
    g_picker_open = 0;
    redraw();
}

static void handle_key(KeySym ks, char ch) {
    if (g_picker_open) {
        if (ks == XK_Escape) { g_picker_open = 0; redraw(); return; }
        if (g_picker_type < 0) {
            /* real wraith-alpha nav for the type-choice list too, not a
             * bespoke scheme - digits/arrows move g_picker_focus (own
             * counter, doesn't touch the outer window's g_focus_nav),
             * Enter activates, exactly like every other nav list in
             * this house. */
            if (ch >= '1' && ch <= '3') { g_picker_focus = ch - '0'; redraw(); }
            else if (ks == XK_Up || ks == XK_Left) { if (g_picker_focus > 1) g_picker_focus--; redraw(); }
            else if (ks == XK_Down || ks == XK_Right || ks == XK_Tab) { if (g_picker_focus < 3) g_picker_focus++; redraw(); }
            else if (ks == XK_Return || ks == XK_KP_Enter) { g_picker_type = g_picker_focus - 1; redraw(); }
            return;
        }
        int single_field = (strcmp(PICKER_TYPES[g_picker_type], "change_gold") == 0);
        char *active = g_active_field == 0 ? g_field1 : g_field2;
        size_t asz = g_active_field == 0 ? sizeof(g_field1) : sizeof(g_field2);
        if (ks == XK_Return || ks == XK_KP_Enter) {
            if (!single_field && g_active_field == 0) { g_active_field = 1; redraw(); return; }
            submit_picker();
            return;
        }
        if (ks == XK_BackSpace) { size_t l = strlen(active); if (l > 0) active[l - 1] = '\0'; redraw(); return; }
        if (ch >= 32 && ch <= 126) {
            size_t l = strlen(active);
            if (l + 1 < asz) { active[l] = ch; active[l + 1] = '\0'; }
            redraw();
            return;
        }
        return;
    }
    if (ch == 'p') return; /* no debug dump wired for events-hq yet */
    if (ks == XK_Return || ks == XK_KP_Enter) {
        if (g_digit_accum > 0 && g_digit_accum <= g_n_nav) g_focus_nav = g_digit_accum;
        g_digit_accum = 0;
        if (g_focus_nav >= 1 && g_focus_nav <= g_n_nav) activate_elem(g_nav[g_focus_nav - 1]);
        return;
    }
    if (ks == XK_Escape) { if (g_digit_accum > 0) { g_digit_accum = 0; return; } g_quit = 1; return; }
    if (ch >= '0' && ch <= '9') {
        int d = ch - '0';
        int new_val = g_digit_accum * 10 + d;
        if (new_val > 0 && new_val <= g_n_nav) { g_digit_accum = new_val; g_focus_nav = new_val; redraw(); }
        else if (d > 0 && d <= g_n_nav) { g_digit_accum = d; g_focus_nav = d; redraw(); }
        else g_digit_accum = 0;
        return;
    }
    if (ks == XK_Up || ks == XK_Left) { if (g_focus_nav > 1) g_focus_nav--; g_digit_accum = 0; redraw(); return; }
    if (ks == XK_Down || ks == XK_Right || ks == XK_Tab) { if (g_focus_nav < g_n_nav) g_focus_nav++; g_digit_accum = 0; redraw(); return; }
    g_digit_accum = 0;
}

/* Real X error found live: rendering the entity's color-emoji glyph
 * (Noto Color Emoji) via Xft/RENDER can hit BadLength on
 * RenderAddGlyphs (large bitmap-strike glyph data), which by default
 * calls exit() and kills the whole editor over one decorative icon.
 * Non-fatal handler so a single bad RENDER request degrades (that one
 * draw call silently fails) instead of crashing the app - the glyph is
 * a "for user sanity" nicety, not something worth losing the whole
 * editor over. */
static int nonfatal_x_error(Display *d, XErrorEvent *e) {
    char buf[128]; XGetErrorText(d, e->error_code, buf, sizeof(buf));
    fprintf(stderr, "events-hq: X error (non-fatal): %s (request %d.%d)\n", buf, e->request_code, e->minor_code);
    return 0;
}

int main(int argc, char **argv) {
    XSetErrorHandler(nonfatal_x_error);
    if (argc < 4) { fprintf(stderr, "usage: %s <house_root> <event_pkg_dir> <entity_label>\n", argv[0]); return 1; }
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_pkg_dir, sizeof(g_pkg_dir), "%s", argv[2]);
    snprintf(g_entity_label, sizeof(g_entity_label), "%s", argv[3]);

    memset(g_close_elem, 0, sizeof(*g_close_elem));
    snprintf(g_close_elem->tag, sizeof(g_close_elem->tag), "closebtn");

    if (access(g_pkg_dir, F_OK) != 0) mkdir(g_pkg_dir, 0755);
    load_pages();
    load_entity_sprite();

    char chtpm_path[PATH_BUF];
    snprintf(chtpm_path, sizeof(chtpm_path), "%s/&.widgits/events-hq/pieces/dashboard.chtpm", g_house_root);
    Elem *window = parse_chtpm(chtpm_path);
    if (!window) { fprintf(stderr, "events-hq: failed to parse %s\n", chtpm_path); return 1; }
    g_window = window;

    char css_path[PATH_BUF];
    snprintf(css_path, sizeof(css_path), "%s/&.widgits/events-hq/pieces/dashboard.css", g_house_root);
    static CssSheet sheet; memset(&sheet, 0, sizeof(sheet));
    css_load(css_path, &sheet);
    g_sheet = &sheet;

    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "events-hq: cannot open display\n"); return 1; }
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);

    refresh_page_data(window);
    layout_pass(window);
    int ww = window->w, wh = window->h;

    /* real managed window + _MOTIF_WM_HINTS decorations=0 - NOT
     * override_redirect, per aug-12-END.txt's own "RESOLVED" section
     * (db-hq's whole keyboard-focus struggle this session). Reused
     * verbatim, not re-derived. */
    XSetWindowAttributes swa;
    swa.background_pixel = WhitePixel(dpy, screen);
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask;
    win = XCreateWindow(dpy, RootWindow(dpy, screen), g_win_x, g_win_y, (unsigned)ww, (unsigned)wh, 0,
                         CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &swa);
    {
        Atom motif_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        long hints[5] = { 2, 0, 0, 0, 0 };
        XChangeProperty(dpy, win, motif_hints, motif_hints, 32, PropModeReplace, (unsigned char *)hints, 5);
        XWMHints *wmhints = XAllocWMHints();
        if (wmhints) { wmhints->flags = InputHint; wmhints->input = True; XSetWMHints(dpy, win, wmhints); XFree(wmhints); }
        Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, win, &wm_delete, 1);
        XClassHint *ch = XAllocClassHint();
        if (ch) { ch->res_name = (char *)"MuchiverseLivedesk"; ch->res_class = (char *)"MuchiverseLivedesk"; XSetClassHint(dpy, win, ch); XFree(ch); }
    }
    XMapRaised(dpy, win);
    XSync(dpy, False);
    { XWindowAttributes wa; if (XGetWindowAttributes(dpy, win, &wa)) { g_win_x = wa.x; g_win_y = wa.y; } }

    gc = XCreateGC(dpy, win, 0, NULL);
    buf = XCreatePixmap(dpy, win, (unsigned)ww, (unsigned)wh, (unsigned)DefaultDepth(dpy, screen));
    xftdraw_buf = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen), cmap);

    redraw();

    while (!g_quit) {
        fd_set fds; FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy); FD_SET(xfd, &fds);
        struct timeval tv = { 0, 150000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                redraw();
            } else if (ev.type == ButtonPress) {
                if (!g_picker_open && ev.xbutton.button == 1 && ev.xbutton.y < CHROME_H &&
                    !(ev.xbutton.x >= g_close_elem->x && ev.xbutton.x < g_close_elem->x + g_close_elem->w &&
                      ev.xbutton.y >= g_close_elem->y && ev.xbutton.y < g_close_elem->y + g_close_elem->h)) {
                    g_dragging = 1; g_drag_last_x = ev.xbutton.x_root; g_drag_last_y = ev.xbutton.y_root;
                }
                if (ev.xbutton.button != 3 && !g_picker_open) handle_click(ev.xbutton.x, ev.xbutton.y);
            } else if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
                g_dragging = 0;
            } else if (ev.type == MotionNotify) {
                if (g_dragging) {
                    int dx = ev.xmotion.x_root - g_drag_last_x, dy = ev.xmotion.y_root - g_drag_last_y;
                    g_win_x += dx; g_win_y += dy;
                    XMoveWindow(dpy, win, g_win_x, g_win_y);
                    g_drag_last_x = ev.xmotion.x_root; g_drag_last_y = ev.xmotion.y_root;
                }
            } else if (ev.type == KeyPress) {
                char buf8[8]; KeySym ks;
                int n = XLookupString(&ev.xkey, buf8, sizeof(buf8) - 1, &ks, NULL);
                buf8[n > 0 ? n : 0] = '\0';
                const char *kname = XKeysymToString(ks);
                snprintf(g_last_key_label, sizeof(g_last_key_label), "%s", kname ? kname : "?");
                handle_key(ks, buf8[0]);
            } else if (ev.type == FocusIn) {
                g_has_real_focus = 1; redraw();
            } else if (ev.type == FocusOut) {
                g_has_real_focus = 0; redraw();
            } else if (ev.type == ClientMessage) {
                Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
                if ((Atom)ev.xclient.data.l[0] == wm_delete) g_quit = 1;
            }
        }
    }

    XftDrawDestroy(xftdraw_buf);
    XFreePixmap(dpy, buf);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
