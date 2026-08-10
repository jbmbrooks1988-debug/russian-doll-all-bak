/* khtpm_plat_win.c — Win32 + WGL shim only (no design logic).
 * All menus/registry/sprites live in khtpm_core.c
 */
#ifndef _WIN32
#error "khtpm_plat_win.c is Windows-only"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <GL/gl.h>
#include "khtpm_core.h"
#include "khtpm_plat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#define POLL_MS 300

static KhtpmEntity *g_e;
static HWND g_hwnd;
static HDC g_hdc;
static HGLRC g_glrc;
static GLuint g_tex;
static int g_has_tex;
static int g_dragging, g_dx, g_dy;

static void upload_tex(void) {
    if (!g_e || !g_e->sprite_pixels) { g_has_tex = 0; return; }
    if (g_tex) glDeleteTextures(1, &g_tex);
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_e->sprite_res, g_e->sprite_res, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_e->sprite_pixels);
    g_has_tex = 1;
}

static void shape_mask(void) {
    int W = g_e->win_px, H = g_e->win_px;
    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (g_e->sprite_pixels && g_e->sprite_res > 0) {
        for (int y = 0; y < H; y++) {
            int sy = (y * g_e->sprite_res) / H;
            if (sy >= g_e->sprite_res) sy = g_e->sprite_res - 1;
            int x = 0;
            while (x < W) {
                int sx = (x * g_e->sprite_res) / W;
                if (sx >= g_e->sprite_res) sx = g_e->sprite_res - 1;
                if (g_e->sprite_pixels[(sy * g_e->sprite_res + sx) * 4 + 3] <= 127) {
                    x++; continue;
                }
                int run = x;
                while (x < W) {
                    sx = (x * g_e->sprite_res) / W;
                    if (sx >= g_e->sprite_res) sx = g_e->sprite_res - 1;
                    if (g_e->sprite_pixels[(sy * g_e->sprite_res + sx) * 4 + 3] <= 127) break;
                    x++;
                }
                HRGN r = CreateRectRgn(run, y, x, y + 1);
                CombineRgn(region, region, r, RGN_OR);
                DeleteObject(r);
            }
        }
    } else {
        HRGN box = CreateRectRgn(0, 0, W, H);
        CombineRgn(region, region, box, RGN_OR);
        DeleteObject(box);
    }
    if (!SetWindowRgn(g_hwnd, region, TRUE)) DeleteObject(region);
}

static void render(void) {
    if (!g_hdc) return;
    glViewport(0, 0, g_e->win_px, g_e->win_px);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (g_has_tex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, -1);
        glTexCoord2f(1, 0); glVertex2f(1, 1);
        glTexCoord2f(0, 0); glVertex2f(-1, 1);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        glBegin(GL_QUADS);
        glColor3f(0.15f, 0.35f, 0.55f);
        glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
        glEnd();
    }
    SwapBuffers(g_hdc);
}

/* Linux dispatch_action: system("%s '%s' &", action, package_dir).
 * Always append package_dir as argv[1] for shell scripts (open_event_ez etc). */
static void run_shell(const char *cmd, const char *package_dir) {
    if (!cmd || !cmd[0]) return;
    char cmdline[KHTPM_PATH_BUF * 4];
    char house[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, house);

    /* Prefer .ps1 twin (open_event_ez.ps1) — relative paths only (emoji house).
     * Linux dispatch always appends package_dir as argv[1]. */
    if (strstr(cmd, ".sh") || strstr(cmd, ".ps1")) {
        char script[KHTPM_PATH_BUF];
        snprintf(script, sizeof(script), "%s", cmd);
        for (char *p = script; *p; p++) if (*p == '/') *p = '\\';
        size_t n = strlen(script);
        if (n > 3 && strcmp(script + n - 3, ".sh") == 0) {
            /* try .ps1 twin first (Win native) */
            char ps1[KHTPM_PATH_BUF];
            snprintf(ps1, sizeof(ps1), "%.*s.ps1", (int)(n - 3), script);
            snprintf(cmdline, sizeof(cmdline),
                     "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\" \"%s\"",
                     ps1, package_dir ? package_dir : ".");
            /* if .ps1 missing CreateProcess still fails — also try bash .sh below via history */
        } else {
            snprintf(cmdline, sizeof(cmdline),
                     "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\" \"%s\"",
                     script, package_dir ? package_dir : ".");
        }
    } else if (strstr(cmd, "env ") || strstr(cmd, "sh ") || strstr(cmd, "button.sh")) {
        /* complex METHOD (ava Events ez): rewrite event-ez if we can detect it */
        if (strstr(cmd, "event-ez") && package_dir) {
            snprintf(cmdline, sizeof(cmdline),
                     "powershell -NoProfile -ExecutionPolicy Bypass -File "
                     "\"*.monads\\*.muchi-pet\\ops\\open_event_ez.ps1\" \"%s\"",
                     package_dir);
            /* ava/asa use event_pkg under package — open_event_ez.ps1 handles entity dir */
        } else {
            snprintf(cmdline, sizeof(cmdline),
                     "bash -lc %s", cmd); /* best-effort */
        }
    } else {
        snprintf(cmdline, sizeof(cmdline), "%s \"%s\"", cmd,
                 package_dir ? package_dir : ".");
    }
do_create:
    {
        STARTUPINFOA si; PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        /* cwd = house so relative @.apps/... and &.widgits resolve */
        BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                                 CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                                 NULL, house, &si, &pi);
        if (!ok) {
            /* retry without DETACHED so console tools can show errors */
            ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            ok = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                                CREATE_NEW_PROCESS_GROUP, NULL, house, &si, &pi);
        }
        if (g_e) khtpm_history(g_e, "run_shell ok=%d cmd=%s", (int)ok, cmdline);
        if (pi.hThread) CloseHandle(pi.hThread);
        if (pi.hProcess) CloseHandle(pi.hProcess);
    }
}

static void show_menu(void); /* fwd */

static void apply_view_action(int idx) {
    if (idx < 0 || idx >= g_e->n_view) return;
    char cmd[KHTPM_PATH_BUF];
    int act = khtpm_menu_apply(g_e, g_e->view[idx].action, cmd, sizeof(cmd));
    if (act == KHTPM_ACT_CLOSE) {
        DestroyWindow(g_hwnd);
        return;
    }
    if (act == KHTPM_ACT_RAISE_MENU) {
        show_menu();
        return;
    }
    if (act == KHTPM_ACT_OPEN_DIR) {
        /* Open package folder in Windows Explorer (METHOD Dir / xdg-open).
         * Use ShellExecute + absolute path — do NOT go through run_shell,
         * which appends package_dir again and breaks explorer. */
        char abs[KHTPM_PATH_BUF];
        abs[0] = '\0';
        if (g_e->package_dir[0]) {
            DWORD n = GetFullPathNameA(g_e->package_dir, (DWORD)sizeof(abs), abs, NULL);
            if (n == 0 || n >= sizeof(abs))
                snprintf(abs, sizeof(abs), "%s", g_e->package_dir);
            for (char *p = abs; *p; p++) if (*p == '/') *p = '\\';
        }
        if (abs[0]) {
            HINSTANCE hi = ShellExecuteA(NULL, "explore", abs, NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)hi <= 32) {
                /* fallback: explorer.exe with quoted path */
                char exp[KHTPM_PATH_BUF * 2];
                snprintf(exp, sizeof(exp), "explorer.exe \"%s\"", abs);
                STARTUPINFOA si; PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                CreateProcessA(NULL, exp, NULL, NULL, FALSE,
                               CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                               NULL, NULL, &si, &pi);
                if (pi.hThread) CloseHandle(pi.hThread);
                if (pi.hProcess) CloseHandle(pi.hProcess);
            }
            khtpm_history(g_e, "OPEN_DIR explorer %s", abs);
        } else {
            khtpm_history(g_e, "OPEN_DIR empty package_dir");
        }
        return;
    }
    if (act == KHTPM_ACT_RUN)
        run_shell(cmd, g_e->package_dir);
}

static void show_menu(void) {
    POINT pt; GetCursorPos(&pt);
    HMENU h = CreatePopupMenu();
    /* header id (Linux shows full_id) */
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "-- %s --", g_e->full_id);
    AppendMenuA(h, MF_STRING | MF_GRAYED, 999, hdr);
    for (int i = 0; i < g_e->n_view; i++)
        AppendMenuA(h, MF_STRING, (UINT)(1000 + i), g_e->view[i].label);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(h, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(h);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, POLL_MS, NULL);
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            if (!khtpm_package_exists(g_e)) { DestroyWindow(hwnd); return 0; }
            int raise = 0, open_menu = 0;
            if (khtpm_relay_poll(g_e, &raise, &open_menu)) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (raise || open_menu) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                SetForegroundWindow(hwnd);
            }
            if (open_menu) {
                khtpm_menu_load(g_e);
                show_menu();
            }
            render();
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_dragging = 1;
        POINT pt; GetCursorPos(&pt);
        RECT rc; GetWindowRect(hwnd, &rc);
        g_dx = pt.x - rc.left;
        g_dy = pt.y - rc.top;
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_dragging && (wp & MK_LBUTTON)) {
            POINT pt; GetCursorPos(&pt);
            SetWindowPos(hwnd, HWND_TOPMOST, pt.x - g_dx, pt.y - g_dy,
                         0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_dragging) {
            ReleaseCapture();
            g_dragging = 0;
            RECT rc; GetWindowRect(hwnd, &rc);
            int x = rc.left, y = rc.top;
            khtpm_pos_clamp(&x, &y, g_e->win_px,
                            GetSystemMetrics(SM_CXSCREEN),
                            GetSystemMetrics(SM_CYSCREEN));
            khtpm_pos_write(g_e, x, y);
            SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_RBUTTONUP:
        khtpm_menu_load(g_e); /* refresh pages from disk */
        show_menu();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= 1000 && id < 1000 + g_e->n_view)
            apply_view_action(id - 1000);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_glrc) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(g_glrc);
            g_glrc = NULL;
        }
        if (g_hdc) { ReleaseDC(hwnd, g_hdc); g_hdc = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void khtpm_plat_ensure_taskbar(const char *house_root) {
    char pidpath[KHTPM_PATH_BUF];
    khtpm_path_join(pidpath, sizeof(pidpath), house_root, "#.desktop/livedesk_taskbar.pid");
    khtpm_path_norm(pidpath);
    FILE *f = fopen(pidpath, "r");
    if (f) {
        int tpid = 0;
        if (fscanf(f, "%d", &tpid) == 1 && tpid > 1) {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)tpid);
            if (h) {
                DWORD code = 0;
                if (GetExitCodeProcess(h, &code) && code == STILL_ACTIVE) {
                    CloseHandle(h); fclose(f); return;
                }
                CloseHandle(h);
            }
        }
        fclose(f);
    }
    char exe[KHTPM_PATH_BUF];
    khtpm_path_join(exe, sizeof(exe), house_root,
                    "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.exe");
    khtpm_path_norm(exe);
    char cmdline[KHTPM_PATH_BUF * 2];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" \".\"", exe);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                   CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                   NULL, NULL, &si, &pi);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
}

int khtpm_plat_run_entity(KhtpmEntity *e) {
    g_e = e;
    g_has_tex = 0;
    g_tex = 0;
    g_dragging = 0;

    int x = 80, y = 80;
    khtpm_pos_read(e, &x, &y);
    khtpm_pos_clamp(&x, &y, e->win_px,
                    GetSystemMetrics(SM_CXSCREEN),
                    GetSystemMetrics(SM_CYSCREEN));

    HINSTANCE hi = GetModuleHandle(NULL);
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "KHTPM_Entity";
    RegisterClassA(&wc);

    char title[300];
    snprintf(title, sizeof(title), "%s %s", e->glyph, e->entity);

    g_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "KHTPM_Entity", title,
        WS_POPUP | WS_VISIBLE,
        x, y, e->win_px, e->win_px,
        NULL, NULL, hi, NULL);
    if (!g_hwnd) return 1;

    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) return 1;
    g_glrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_glrc);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    upload_tex();
    if (g_has_tex) shape_mask();

    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(g_hwnd, SW_SHOW);
    render();
    khtpm_history(e, "plat_win open (core-driven)");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
