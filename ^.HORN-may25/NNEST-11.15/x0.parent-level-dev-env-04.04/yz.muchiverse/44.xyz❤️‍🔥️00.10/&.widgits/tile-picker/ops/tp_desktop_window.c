/* tp_desktop_window - the missing "live" half of tp_place_desktop.
 * Usage: tp_desktop_window.+x <package_dir>
 *
 * tp_place_desktop.c already writes a real package (glyph.txt + meta.pdl)
 * into #.desktop/tiles/<name>/, but nothing rendered it - it just sat on
 * disk. This is a small, borderless GLX window (same override_redirect
 * technique as 01.muchi-pals's egg_window.c, stripped down to the parts a
 * tile stamp actually needs: no sprite/vitals/self-tick, just "be a real,
 * draggable, closeable OS window that represents this one desktop
 * package"). Window title is the glyph itself so it's identifiable at a
 * glance without needing real glyph-texture rendering (deferred - see
 * aomorai-editor-blueprint.md open question 8 on glyph widening; this
 * window already reads glyph.txt as a whole line so widening it to UTF-8
 * later is a non-issue for this file specifically).
 *
 * Dragging repositions the window and writes the pixel position back to
 * <package_dir>/desktop_pos.txt (x=,y=) - the one deliberate state write,
 * same "recording user input, not deciding behavior" exception egg_window's
 * own header comment makes for its own position write-back.
 *
 * Polls every ~300ms for <package_dir>/glyph.txt still existing - if the
 * package directory has been removed (e.g. tp_import_from_desktop.+x with
 * a future --remove flag, or a human rm -rf), this window closes itself
 * instead of floating forever pointing at nothing.
 */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#define WIN_PX 64
#define POLL_INTERVAL_USEC 300000
#define PATH_BUF 4352
/* REAL FIX 2026-08-04, direct instruction ("desk has a grid... egg-pets
 * snap to grid... make sure windows/procs are killed and don't render
 * more than 30fps, cpu is getting hot"): same GRID_CELL_PX egg_window.c
 * uses (01.muchi-pals-🥚️-13.01/system/egg_window.c) - tile stamps should
 * snap to the SAME desktop grid egg-pals already use, not a separate
 * one. */
#define GRID_CELL_PX 80
#define MAX_FPS 30
#define MIN_FRAME_USEC (1000000 / MAX_FPS)

static void read_glyph(const char *package_dir, char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(out, out_sz, f)) out[strcspn(out, "\r\n")] = '\0';
    fclose(f);
}

static int package_still_exists(const char *package_dir) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/glyph.txt", package_dir);
    struct stat st;
    return stat(path, &st) == 0;
}

/* Cheap, deterministic glyph->color hash so different tile stamps are
 * visually distinct even before/without a loaded font. */
static void glyph_color(char g, float *r, float *gg, float *b) {
    unsigned int h = (unsigned int)(unsigned char)g * 2654435761u;
    *r = ((h >> 0) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *gg = ((h >> 8) & 0xFF) / 255.0f * 0.7f + 0.2f;
    *b = ((h >> 16) & 0xFF) / 255.0f * 0.7f + 0.2f;
}

/* REAL, NEW 2026-08-04, direct instruction ("next test, it should be a
 * real tile" - i.e. the window must show the actual glyph, not just a
 * colored square). No sprite/texture pipeline exists for tile-picker yet
 * (that's the still-open glyph-widening/rendering question in both
 * design docs), so this uses the same glXUseXFont technique GLUT's own
 * bitmap-font helpers are built on: load a real X core font, convert its
 * glyphs into GL display lists, draw with glCallLists. "fixed"-family X
 * core fonts are always present on any X11 install (confirmed via
 * xlsfonts on this machine) - no extra font file dependency. Returns 0
 * and leaves *base_list unset if no such font can be loaded (caller falls
 * back to the plain color square, same as before this fix). */
static int g_font_loaded = 0;
static GLuint g_font_base = 0;
static XFontStruct *g_font_info = NULL;

static int load_glyph_font(Display *dpy) {
    g_font_info = XLoadQueryFont(dpy, "-sony-fixed-medium-r-normal--24-170-100-100-c-120-iso8859-1");
    if (!g_font_info) g_font_info = XLoadQueryFont(dpy, "fixed");
    if (!g_font_info) return 0;
    g_font_base = glGenLists(256);
    glXUseXFont(g_font_info->fid, 0, 256, g_font_base);
    return 1;
}

static void draw_glyph(char g) {
    glColor3f(0.05f, 0.05f, 0.05f);
    /* Roughly center a single ~14px-wide glyph in the 64px window. */
    glRasterPos2f(-0.22f, -0.3f);
    glListBase(g_font_base);
    unsigned char c = (unsigned char)g;
    glCallLists(1, GL_UNSIGNED_BYTE, &c);
}

/* REAL FIX 2026-08-04, direct instruction ("do u see how egg-pal creates
 * the same emoji that user picked?"): load_sprite/upload_texture/
 * draw_sprite below are the same real technique 01.muchi-pals's
 * egg_window.c already uses to render a picked emoji as a real textured
 * quad, not text. tp_place_desktop.c now generates <package_dir>/
 * sprite.csv via the same emoji_gen_atlas.+x -> emoji_xtract.+x pipeline
 * hatch_egg.c uses; this window loads that CSV if present and draws the
 * real emoji texture instead of the glyph-hashed color square + font
 * fallback (which remains the fallback if sprite.csv is missing/failed
 * to generate). */
static GLuint g_sprite_texture = 0;
static int g_has_sprite = 0;
/* REAL FIX 2026-08-04, direct instruction ("asa's transparency isn't
 * fixed, just background is now red"): GL_BLEND alone only blends
 * within the GL scene against whatever this window itself already drew
 * (the glClearColor fill) - it does NOT make the real X11 WINDOW
 * transparent to the desktop behind it. Plain X11 windows are opaque
 * rectangles by default; real per-pixel desktop transparency needs the
 * X11 Shape Extension to cut the window's actual SHAPE to match the
 * sprite's alpha - exactly what egg_window.c's own build_shape_mask()
 * already does (ported verbatim below, POSIX branch only, matching this
 * file's own X11/GLX-only scope). Sprite pixels are now kept around
 * (not freed after texture upload) so the mask can be built from real
 * alpha data - same "kept around... so the shape mask can be rebuilt"
 * comment egg_window.c's own header already has for the identical
 * reason. */
static unsigned char *g_sprite_pixels = NULL;
static int g_sprite_res = 0;

static int load_sprite_csv(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }

    unsigned char *pixels = malloc((size_t)res * (size_t)res * 4);
    if (!pixels) { fclose(f); return 0; }

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
    if (count != res * res) { free(pixels); return 0; }

    glGenTextures(1, &g_sprite_texture);
    glBindTexture(GL_TEXTURE_2D, g_sprite_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res, res, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    g_sprite_pixels = pixels; /* kept, not freed - see comment above */
    g_sprite_res = res;
    return 1;
}

/* Verbatim port of egg_window.c's own build_shape_mask() (POSIX/X11
 * branch) - builds the window's real clip shape from the sprite's own
 * alpha channel (upscaled nearest-neighbor to the window's pixel size),
 * so the desktop genuinely shows through transparent pixels instead of
 * this window's own opaque background fill. */
static void build_shape_mask(Display *dpy, Window win, GC mask_gc, Pixmap mask) {
    XSetForeground(dpy, mask_gc, 0);
    XFillRectangle(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX);
    XSetForeground(dpy, mask_gc, 1);
    if (g_sprite_pixels) {
        for (int y = 0; y < WIN_PX; y++) {
            int sy = (y * g_sprite_res) / WIN_PX;
            if (sy >= g_sprite_res) sy = g_sprite_res - 1;
            for (int x = 0; x < WIN_PX; x++) {
                int sx = (x * g_sprite_res) / WIN_PX;
                if (sx >= g_sprite_res) sx = g_sprite_res - 1;
                if (g_sprite_pixels[(sy * g_sprite_res + sx) * 4 + 3] > 127) {
                    XFillRectangle(dpy, mask, mask_gc, x, y, 1, 1);
                }
            }
        }
    } else {
        XFillArc(dpy, mask, mask_gc, 0, 0, WIN_PX, WIN_PX, 0, 360 * 64);
    }
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
}

static void draw_sprite(void) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_sprite_texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/* REAL, NEW 2026-08-04, direct instruction ("^ mode... wherever they
 * click... the phymoji will appear"): if tp_place_desktop.c already
 * wrote a desktop_pos.txt (a real click point resolved by
 * tp_arm_placer.c), spawn there instead of the fixed grid default -
 * "wherever they click" means the window's own life should start at
 * that point, not just be draggable to it afterward. */
static int read_initial_pos(const char *package_dir, int *out_x, int *out_y) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[128];
    int x = -1, y = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) x = atoi(line + 2);
        else if (strncmp(line, "y=", 2) == 0) y = atoi(line + 2);
    }
    fclose(f);
    if (x < 0 || y < 0) return 0;
    *out_x = x; *out_y = y;
    return 1;
}

/* REAL, NEW 2026-08-04, direct instruction ("add the context menus that
 * already exist from egg-pal to these by default"): a real, data-driven
 * popup context menu, modeled directly on egg_window.c's own
 * open_context_menu()/draw_context_menu()/close_context_menu() (same
 * override_redirect popup + XGrabPointer-on-open technique), but
 * reading its item list from the package's own meta.pdl METHOD rows
 * instead of egg_window's single hardcoded "Close" - see
 * TILE_PICKER_DESIGN.md §4.5. tp_place_desktop.c writes a default
 * METHOD row ("Close") into every new package, so the default behavior
 * matches egg_window's own exactly - this just makes the list
 * extensible (more methods appended to meta.pdl later, e.g. "Open Event
 * Editor" once event-editor exists, need no renderer changes here). */
#define MAX_METHODS 8
#define POPUP_ROW_H 28
#define POPUP_W 160

/* action widened 2026-08-04, direct instruction (fo-menu-sys.md's real
 * convention: VALUE is a real, directly-executable command, e.g. a full
 * "gnome-terminal -- \"<long absolute house path>/chat.sh\"" line, not a
 * short keyword) - 64 bytes silently truncated/dropped real rows built
 * from this house's own long, emoji-heavy absolute paths. */
typedef struct { char label[64]; char action[PATH_BUF]; } MethodItem;

static int load_methods(const char *package_dir, MethodItem *items, int max) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/meta.pdl", package_dir);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t llen = (size_t)(label_end - p);
        if (llen == 0 || llen >= sizeof(items[0].label)) continue;
        memcpy(items[n].label, p, llen);
        items[n].label[llen] = '\0';

        char *a = end + 1;
        while (*a == ' ') a++;
        char *a_end = a + strcspn(a, "\r\n");
        while (a_end > a && a_end[-1] == ' ') a_end--;
        size_t alen = (size_t)(a_end - a);
        if (alen == 0 || alen >= sizeof(items[0].action)) continue;
        memcpy(items[n].action, a, alen);
        items[n].action[alen] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

static Window open_context_menu(Display *dpy, GC gc, int root_x, int root_y, int nitems) {
    int h = POPUP_ROW_H * (nitems > 0 ? nitems : 1);
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = WhitePixel(dpy, DefaultScreen(dpy));
    swa.event_mask = ExposureMask | ButtonPressMask;
    Window popup = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                                  root_x, root_y, POPUP_W, h, 1,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XMapRaised(dpy, popup);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));
    XGrabPointer(dpy, popup, True, ButtonPressMask, GrabModeAsync, GrabModeAsync,
                 None, None, CurrentTime);
    return popup;
}

static void draw_context_menu(Display *dpy, Window popup, GC gc, MethodItem *items, int n) {
    XClearWindow(dpy, popup);
    int h = POPUP_ROW_H * (n > 0 ? n : 1);
    XDrawRectangle(dpy, popup, gc, 0, 0, POPUP_W - 1, h - 1);
    for (int i = 0; i < n; i++) {
        if (i > 0) XDrawLine(dpy, popup, gc, 0, i * POPUP_ROW_H, POPUP_W, i * POPUP_ROW_H);
        XDrawString(dpy, popup, gc, 12, i * POPUP_ROW_H + POPUP_ROW_H / 2 + 4,
                    items[i].label, (int)strlen(items[i].label));
    }
}

static void close_context_menu(Display *dpy, Window popup) {
    XUngrabPointer(dpy, CurrentTime);
    XDestroyWindow(dpy, popup);
}

/* REAL, NEW 2026-08-04, direct instruction ("allow user editing of
 * asset... place an asset in asset folder of entity, and in .pal
 * specify emoji other than default, or path of asset (jpg/png), it will
 * use that instead of default emoji"). Real, simple convention:
 * <package_dir>/asset.pal, key=value lines:
 *   glyph=<emoji>          - regenerate sprite.csv from a DIFFERENT
 *                            emoji than glyph.txt's own default, via the
 *                            same real emoji_gen_atlas/emoji_xtract
 *                            pipeline tp_place_desktop.c already uses.
 *   asset_path=<path>      - use a real, arbitrary user image (PNG/JPG,
 *                            any size - NOT required to be pre-cropped
 *                            to 64x64) instead of any emoji at all, via
 *                            the NEW tp_asset_to_sprite.+x (see that
 *                            file's own header for why emoji_xtract.+x
 *                            alone isn't the right tool for an arbitrary
 *                            user image). Relative paths resolve against
 *                            package_dir/assets/ (the real "asset
 *                            folder" named in the instruction); absolute
 *                            paths are used as-is (e.g. pointing
 *                            directly at a file elsewhere, without
 *                            moving it).
 * Regenerates sprite.csv in place, once, at window startup - editing
 * asset.pal and relaunching the window picks up the change. */
static void apply_asset_override(const char *package_dir, const char *ops_dir) {
    char asset_pal[PATH_BUF];
    snprintf(asset_pal, sizeof(asset_pal), "%s/asset.pal", package_dir);
    FILE *f = fopen(asset_pal, "r");
    if (!f) return;

    char glyph_override[64] = "", asset_path_raw[PATH_BUF] = "";
    char line[PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "glyph=", 6) == 0) snprintf(glyph_override, sizeof(glyph_override), "%s", line + 6);
        else if (strncmp(line, "asset_path=", 11) == 0) snprintf(asset_path_raw, sizeof(asset_path_raw), "%s", line + 11);
    }
    fclose(f);

    char sprite_path[PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);

    if (asset_path_raw[0]) {
        char resolved[PATH_BUF];
        if (asset_path_raw[0] == '/') {
            snprintf(resolved, sizeof(resolved), "%s", asset_path_raw);
        } else {
            snprintf(resolved, sizeof(resolved), "%s/assets/%s", package_dir, asset_path_raw);
        }
        char cmd[PATH_BUF * 2];
        snprintf(cmd, sizeof(cmd), "'%s/tp_asset_to_sprite.+x' '%s' 64 '%s' >/dev/null 2>&1",
                 ops_dir, resolved, sprite_path);
        int rc = system(cmd);
        (void)rc;
    } else if (glyph_override[0]) {
        char atlas_path[PATH_BUF], cmd[PATH_BUF * 2];
        snprintf(atlas_path, sizeof(atlas_path), "%s/atlas_override.png", package_dir);
        snprintf(cmd, sizeof(cmd),
                 "'%s/emoji_gen_atlas.+x' '%s' '%s' >/dev/null 2>&1 && "
                 "'%s/emoji_xtract.+x' '%s' 0 64 '%s' >/dev/null 2>&1",
                 ops_dir, glyph_override, atlas_path, ops_dir, atlas_path, sprite_path);
        int rc = system(cmd);
        (void)rc;
    }
}

static void write_pos(const char *package_dir, int x, int y) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/desktop_pos.txt", package_dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "x=%d\ny=%d\n", x, y);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_desktop_window.+x <package_dir>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    /* REAL FIX 2026-08-04, direct instruction ("id like to see emojis
     * tho"): glyph.txt can now hold a real multi-byte UTF-8 emoji -
     * kept as a full string, not truncated to its first byte. Real
     * texture rendering of that emoji (via emoji_gen_atlas.+x, this
     * house's own FreeType+NotoColorEmoji pipeline, already used by
     * chtpm_rgb_render) is NOT done here yet - this window still only
     * shows a glyph-hashed color + the glyph as its window title, same
     * as before this fix. Flagged as still-open, tracked alongside the
     * tp_menu_input.c/tp_compose_frame.c glyph-widening this session
     * already did for the picker's own chtpm-rendered path (which DOES
     * show real emoji, via that same already-working renderer). */
    char glyph_line[64];
    read_glyph(package_dir, glyph_line, sizeof(glyph_line));
    char glyph = glyph_line[0] ? glyph_line[0] : '?';
    const char *glyph_str = glyph_line[0] ? glyph_line : "?";

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "tp_desktop_window: cannot open display\n");
        return 1;
    }

    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), att);
    if (!vi) {
        fprintf(stderr, "tp_desktop_window: no suitable visual\n");
        return 1;
    }

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | KeyPressMask;
    swa.override_redirect = True;

    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 3 * GRID_CELL_PX, 3 * GRID_CELL_PX, WIN_PX, WIN_PX,
                                0, vi->depth, InputOutput, vi->visual,
                                CWColormap | CWEventMask | CWOverrideRedirect, &swa);
    XMapWindow(dpy, win);
    char title[80];
    snprintf(title, sizeof(title), "tile:%s", glyph_str);
    XStoreName(dpy, win, title);

    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    /* REAL BUG, found + fixed 2026-08-04, direct instruction ("show
     * them with transparent png background removed"): GL_BLEND was
     * never enabled anywhere in this file - a loaded sprite's real
     * alpha channel (transparency) was being ignored entirely, so a
     * PNG's transparent background rendered as opaque instead of
     * showing the glyph-hashed background color through it. Same real
     * fix egg_window.c's own header already applies for the identical
     * reason ("eggshell background behind transparent sprite pixels"). */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float r, g, b;
    glyph_color(glyph, &r, &g, &b);
    g_font_loaded = load_glyph_font(dpy);

    /* Resolve ops_dir (same /proc/self/exe technique tp_place_desktop.c
     * already uses) so apply_asset_override() can find tp_asset_to_
     * sprite.+x/emoji_gen_atlas.+x/emoji_xtract.+x next to this binary. */
    {
        char self_path[PATH_BUF];
        ssize_t slen = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (slen > 0) {
            self_path[slen] = '\0';
            char *ops_dir = dirname(self_path);
            apply_asset_override(package_dir, ops_dir);
        }
    }

    char sprite_path[PATH_BUF];
    snprintf(sprite_path, sizeof(sprite_path), "%s/sprite.csv", package_dir);
    g_has_sprite = load_sprite_csv(sprite_path);

    /* Real window shape from the sprite's own alpha, if we have one -
     * see build_shape_mask()'s own header comment for why GL_BLEND
     * alone wasn't enough. */
    if (g_has_sprite) {
        Pixmap shape_mask = XCreatePixmap(dpy, win, WIN_PX, WIN_PX, 1);
        GC shape_gc = XCreateGC(dpy, shape_mask, 0, NULL);
        build_shape_mask(dpy, win, shape_gc, shape_mask);
        XFreeGC(dpy, shape_gc);
        XFreePixmap(dpy, shape_mask);
    }

    int screen_w = DisplayWidth(dpy, DefaultScreen(dpy));
    int screen_h = DisplayHeight(dpy, DefaultScreen(dpy));
    int max_col = (screen_w / GRID_CELL_PX) - 1;
    int max_row = (screen_h / GRID_CELL_PX) - 1;
    if (max_col < 0) max_col = 0;
    if (max_row < 0) max_row = 0;

    int xfd = ConnectionNumber(dpy);
    int win_x = 3 * GRID_CELL_PX, win_y = 3 * GRID_CELL_PX; /* grid-aligned spawn, matching egg_window.c's own default */
    {
        int ix, iy;
        if (read_initial_pos(package_dir, &ix, &iy)) {
            int gx = (ix + GRID_CELL_PX / 2) / GRID_CELL_PX;
            int gy = (iy + GRID_CELL_PX / 2) / GRID_CELL_PX;
            if (gx < 0) gx = 0; if (gx > max_col) gx = max_col;
            if (gy < 0) gy = 0; if (gy > max_row) gy = max_row;
            win_x = gx * GRID_CELL_PX;
            win_y = gy * GRID_CELL_PX;
            XMoveWindow(dpy, win, win_x, win_y);
        }
    }
    MethodItem methods[MAX_METHODS];
    int n_methods = load_methods(package_dir, methods, MAX_METHODS);
    if (n_methods == 0) {
        snprintf(methods[0].label, sizeof(methods[0].label), "Close");
        snprintf(methods[0].action, sizeof(methods[0].action), "CLOSE");
        n_methods = 1;
    }
    Window popup_win = 0;
    GC popup_gc = XCreateGC(dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, NULL);

    int dragging = 0, drag_start_x = 0, drag_start_y = 0;
    int running = 1;
    struct timeval last_frame = { 0, 0 };

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, POLL_INTERVAL_USEC };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        int need_redraw = 0;
        while (XPending(dpy)) {
            XEvent xev;
            XNextEvent(dpy, &xev);
            if (popup_win && xev.type == Expose && xev.xany.window == popup_win) {
                draw_context_menu(dpy, popup_win, popup_gc, methods, n_methods);
            } else if (popup_win && xev.type == ButtonPress) {
                /* Pointer is grabbed to popup_win while it's open (see
                 * open_context_menu) - every button press anywhere
                 * arrives here regardless of which window it physically
                 * landed on, same real technique egg_window.c's own
                 * popup uses. */
                int row = xev.xbutton.y / POPUP_ROW_H;
                int inside = xev.xbutton.x >= 0 && xev.xbutton.x < POPUP_W &&
                             row >= 0 && row < n_methods;
                close_context_menu(dpy, popup_win);
                popup_win = 0;
                if (inside) {
                    /* REAL FIX 2026-08-04, direct instruction: this house
                     * already has a real, canonical convention for
                     * method dispatch - #.haiku+/tpmos-re-dox/fo-menu-
                     * sys.md ("Fuzz-Op Dynamic Menu System"): a
                     * METHOD's VALUE is a real, directly-executable op
                     * path (e.g. "projects/fuzz-op/ops/+x/feed_op.+x
                     * xlector"), or the literal keyword "void" for an
                     * internal-only no-op - NOT an abstract action
                     * keyword string-matched in C (the previous CLOSE/
                     * CANCEL/PLAY/STOP scheme this replaces). "CLOSE"
                     * stays a single reserved, internal keyword (closing
                     * THIS renderer's own event loop is not something an
                     * external process can do on this process's behalf)
                     * - everything else, including "void", is real
                     * fo-menu-sys.md-style dispatch: void = do nothing,
                     * any other VALUE = a real command line, executed
                     * with this package's own dir as argv[1] (same
                     * "pass the piece_id" convention fo-menu-sys.md's own
                     * examples show). */
                    if (strcmp(methods[row].action, "CLOSE") == 0) {
                        running = 0;
                    } else if (strcmp(methods[row].action, "void") == 0) {
                        /* Intentional no-op - e.g. "Cancel" - discoverable
                         * way to dismiss the menu with zero effect. */
                    } else {
                        char cmd[PATH_BUF * 2];
                        snprintf(cmd, sizeof(cmd), "%s '%s' >/dev/null 2>&1 &",
                                 methods[row].action, package_dir);
                        int rc = system(cmd);
                        (void)rc;
                    }
                }
                need_redraw = 1;
            } else if (xev.type == Expose) {
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
                dragging = 1;
                drag_start_x = xev.xbutton.x_root;
                drag_start_y = xev.xbutton.y_root;
            } else if (xev.type == ButtonRelease && xev.xbutton.button == 1) {
                dragging = 0;
                /* REAL FIX 2026-08-04, direct instruction ("egg-pets
                 * snap to grid... do u see that logic"): same
                 * round-to-nearest-cell technique egg_window.c's own
                 * ButtonRelease handler uses, same GRID_CELL_PX (80),
                 * so a placed tile lands on the identical desktop grid
                 * egg-pals already use, not a separate/incompatible one. */
                int grid_x = (win_x + GRID_CELL_PX / 2) / GRID_CELL_PX;
                int grid_y = (win_y + GRID_CELL_PX / 2) / GRID_CELL_PX;
                if (grid_x < 0) grid_x = 0;
                if (grid_x > max_col) grid_x = max_col;
                if (grid_y < 0) grid_y = 0;
                if (grid_y > max_row) grid_y = max_row;
                win_x = grid_x * GRID_CELL_PX;
                win_y = grid_y * GRID_CELL_PX;
                XMoveWindow(dpy, win, win_x, win_y);
                write_pos(package_dir, win_x, win_y);
                need_redraw = 1;
            } else if (xev.type == ButtonPress && xev.xbutton.button == 3) {
                /* REAL, NEW 2026-08-04: right-click now opens the real
                 * data-driven context menu (see load_methods() above)
                 * instead of closing immediately - matches the ask
                 * ("add the context menus that already exist from
                 * egg-pal to these by default"). */
                if (!popup_win) {
                    popup_win = open_context_menu(dpy, popup_gc,
                                                    xev.xbutton.x_root, xev.xbutton.y_root, n_methods);
                }
            } else if (xev.type == MotionNotify && dragging) {
                int dx = xev.xmotion.x_root - drag_start_x;
                int dy = xev.xmotion.y_root - drag_start_y;
                win_x += dx; win_y += dy;
                XMoveWindow(dpy, win, win_x, win_y);
                drag_start_x = xev.xmotion.x_root;
                drag_start_y = xev.xmotion.y_root;
                need_redraw = 1;
            } else if (xev.type == KeyPress) {
                running = 0;
            }
        }
        if (!running) break;
        if (!package_still_exists(package_dir)) break; /* package gone -> stop pointing at nothing */

        /* REAL FIX 2026-08-04, direct instruction ("dont render more
         * than 30fps etc, cpu is getting hot"): explicit, measured frame
         * pacing (not just relying on select()'s 300ms timeout, which
         * doesn't bound how often this loop redraws once real X events
         * start arriving, e.g. during a drag). Skips the redraw entirely
         * if nothing changed AND less than one frame interval has
         * passed since the last real swap - a static, undragged tile
         * does zero GL work between polls. */
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed_usec = (now.tv_sec - last_frame.tv_sec) * 1000000L +
                             (now.tv_usec - last_frame.tv_usec);
        if (elapsed_usec < MIN_FRAME_USEC) continue; /* hard-cap at MAX_FPS, even mid-drag */
        (void)need_redraw;

        glViewport(0, 0, WIN_PX, WIN_PX);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (g_has_sprite) draw_sprite();
        else if (g_font_loaded) draw_glyph(glyph);
        glXSwapBuffers(dpy, win);
        last_frame = now;
    }

    if (popup_win) close_context_menu(dpy, popup_win); /* e.g. closed via keypress while menu was still open */
    XFreeGC(dpy, popup_gc);
    free(g_sprite_pixels);
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
