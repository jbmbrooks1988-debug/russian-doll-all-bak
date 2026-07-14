/* egg_window.c - shaped GL window, now drawing a real pet sprite instead
 * of a flat fill. Base technique (X11 Shape Extension + GLX, borderless
 * override_redirect, hand-rolled drag) is unchanged from mutaclsym's
 * !.shape=on.0.0Ⓜ️/shape-a0.c - still a CIRCLE, not an egg silhouette yet;
 * swap the mask in the XFillArc call below for that (unequal
 * width/height arcs, or a hand-built XFillPolygon point list). See
 * egg-pals.txt sec. 3 and dox/01-architecture.md step 3.
 *
 * New: reads pieces/world_01/map_lobby/<pet_id>/sprite.csv (written by
 * ops/hatch_egg.c via the emoji_gen_atlas/emoji_xtract pipeline) as
 * plain-text RGBA pixel rows and uploads it as a GL texture, drawn as a
 * full-viewport textured quad - the X11 shape mask clips it to the
 * window's silhouette, so no circular geometry is needed in GL itself.
 * Falls back to the original flat-color circle if no pet_id is given or
 * its sprite.csv can't be read (e.g. run standalone for a quick check:
 * ./system/egg_window).
 *
 * This process must stay a dumb renderer + input relay per egg-pals.txt
 * - do not put pet decision-making in here, only drawing + (later)
 * appending raw click/drag events to a history file for prisc+x to read. */
#define _DEFAULT_SOURCE /* glibc gates M_PI in math.h behind this under -std=c11 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WIDTH 400
#define HEIGHT 400
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";
static GLuint g_texture = 0;
static int g_has_texture = 0;

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) { snprintf(project_root, sizeof(project_root), "%s", env); return; }
    if (!getcwd(project_root, sizeof(project_root))) snprintf(project_root, sizeof(project_root), ".");
}

static int load_sprite(const char *csv_path, unsigned char **out_pixels, int *out_res) {
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

    if (count != res * res) { free(pixels); return 0; }
    *out_pixels = pixels;
    *out_res = res;
    return 1;
}

static void upload_texture(const unsigned char *pixels, int res) {
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res, res, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    g_has_texture = 1;
}

static void draw_circle(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(0.2f, 0.6f, 0.9f); // Solid blue color
    glVertex2f(0.0f, 0.0f);      // Center
    for (int i = 0; i <= 100; i++) {
        float angle = i * 2.0f * (float)M_PI / 100;
        glVertex2f(cosf(angle), sinf(angle));
    }
    glEnd();
}

static void draw_sprite(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

int main(int argc, char **argv) {
    const char *pet_id = (argc >= 2) ? argv[1] : NULL;
    resolve_root();

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    // 1. Choose visual for OpenGL
    GLint att[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), att);

    // 2. Set window attributes
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonMotionMask;
    swa.override_redirect = True; // Tells Window Manager to remove borders/titlebar

    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 300, 300, WIDTH, HEIGHT,
                              0, vi->depth, InputOutput, vi->visual,
                              CWColormap | CWEventMask | CWOverrideRedirect, &swa);

    XMapWindow(dpy, win);
    XStoreName(dpy, win, pet_id ? pet_id : "Round GL Window");

    // 3. Create a circular bounding mask using X11 Shape Extension
    Pixmap mask = XCreatePixmap(dpy, win, WIDTH, HEIGHT, 1);
    GC gc = XCreateGC(dpy, mask, 0, NULL);

    XSetForeground(dpy, gc, 0); // Clear mask (0 = completely transparent/clipped)
    XFillRectangle(dpy, mask, gc, 0, 0, WIDTH, HEIGHT);

    XSetForeground(dpy, gc, 1); // 1 = Visible window area
    XFillArc(dpy, mask, gc, 0, 0, WIDTH, HEIGHT, 0, 360 * 64);

    // Apply the circle shape to the window boundary
    XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);

    // 4. Setup OpenGL Context
    GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);

    glClearColor(0.95f, 0.90f, 0.80f, 1.0f); /* eggshell background behind transparent sprite pixels */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (pet_id) {
        char sprite_path[PATH_BUF];
        snprintf(sprite_path, sizeof(sprite_path), "%s/pieces/world_01/map_lobby/%s/sprite.csv", project_root, pet_id);
        unsigned char *pixels = NULL;
        int res = 0;
        if (load_sprite(sprite_path, &pixels, &res)) {
            upload_texture(pixels, res);
            free(pixels);
        } else {
            fprintf(stderr, "egg_window: could not load sprite for %s, falling back to circle\n", pet_id);
        }
    }

    // 5. Main Loop
    XEvent xev;
    int drag_start_x = 0, drag_start_y = 0;
    int win_start_x = 300, win_start_y = 300;

    while (1) {
        XNextEvent(dpy, &xev);

        if (xev.type == Expose) {
            glViewport(0, 0, WIDTH, HEIGHT);
            if (g_has_texture) draw_sprite();
            else draw_circle();
            glXSwapBuffers(dpy, win);
        }
        else if (xev.type == ButtonPress && xev.xbutton.button == 1) {
            drag_start_x = xev.xbutton.x_root;
            drag_start_y = xev.xbutton.y_root;
        }
        else if (xev.type == MotionNotify) {
            int dx = xev.xmotion.x_root - drag_start_x;
            int dy = xev.xmotion.y_root - drag_start_y;
            win_start_x += dx;
            win_start_y += dy;
            XMoveWindow(dpy, win, win_start_x, win_start_y);
            drag_start_x = xev.xmotion.x_root;
            drag_start_y = xev.xmotion.y_root;
        }
        else if (xev.type == KeyPress) {
            break; // Press any key to close
        }
    }

    // Cleanup
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
