/* LEGACY: do not add design logic here. Shared = khtpm_taskbar_core.c (+ plat_win/x11). See KHTPM-ARCH.txt */
/* tp_taskbar_win.c â€” Win32 livedesk taskbar (L2.1 nav-ish parity).
 * Usage: tp_taskbar <house_root>
 *
 * Tabs from livedesk_open.txt; click activates entity.
 * Digit buffer + Enter jumps to tab INDEX/NAV (Linux taskbar Nav parity lite).
 * $ runs $.crypts/button.ps1; X closes bar.
 * Full Linux soft-focus / ACTIVATE_NAV menu-row parity still deferred.
 */
#ifndef _WIN32
#error "Windows-only"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <process.h>

#define PATH_BUF 4352
#define BAR_H 36
#define TAB_W 140
#define MAX_TABS 64
#define POLL_MS 400

typedef struct {
    int pid;
    int index;
    char entity[128];
    char path[PATH_BUF];
} Tab;

static char g_house[PATH_BUF];
static char g_pidpath[PATH_BUF];
static Tab g_tabs[MAX_TABS];
static int g_ntabs = 0;
static HWND g_hwnd = NULL;
static char g_nav_buf[16];
static int g_nav_len = 0;

static void join2(char *out, size_t n, const char *a, const char *b) {
    size_t al = strlen(a);
    if (strcmp(a, ".") == 0) snprintf(out, n, "%s", b);
    else if (al && (a[al - 1] == '/' || a[al - 1] == '\\'))
        snprintf(out, n, "%s%s", a, b);
    else
        snprintf(out, n, "%s\\%s", a, b);
    for (char *p = out; *p; p++) if (*p == '/') *p = '\\';
}

static FILE *fopen_u8(const char *utf8, const char *mode) {
    wchar_t wp[PATH_BUF], wm[32];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wp, PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, utf8, -1, wp, PATH_BUF))
        return fopen(utf8, mode);
    MultiByteToWideChar(CP_ACP, 0, mode, -1, wm, 32);
    return _wfopen(wp, wm);
}

static void reload_tabs(void) {
    g_ntabs = 0;
    char path[PATH_BUF];
    join2(path, sizeof(path), g_house, "#.desktop\\livedesk_open.txt");
    FILE *f = fopen_u8(path, "r");
    if (!f) return;
    char line[PATH_BUF];
    while (g_ntabs < MAX_TABS && fgets(line, sizeof(line), f)) {
        Tab *t = &g_tabs[g_ntabs];
        memset(t, 0, sizeof(*t));
        char *p;
        if ((p = strstr(line, "PID="))) t->pid = atoi(p + 4);
        if ((p = strstr(line, "INDEX="))) t->index = atoi(p + 6);
        if ((p = strstr(line, "ENTITY="))) sscanf(p + 7, "%127[^|\r\n]", t->entity);
        if ((p = strstr(line, "PATH="))) {
            snprintf(t->path, sizeof(t->path), "%s", p + 5);
            t->path[strcspn(t->path, "\r\n")] = 0;
        }
        if (t->pid > 0 && t->entity[0]) g_ntabs++;
    }
    fclose(f);
}

static void write_pidfile(void) {
    FILE *f = fopen_u8(g_pidpath, "w");
    if (!f) return;
    fprintf(f, "%d\n", (int)GetCurrentProcessId());
    fclose(f);
}

static void activate_tab(int i) {
    if (i < 0 || i >= g_ntabs) return;
    char relay[PATH_BUF];
    join2(relay, sizeof(relay), g_tabs[i].path, "interact_relay.txt");
    FILE *f = fopen_u8(relay, "w");
    if (f) {
        /* Linux uses ACTIVATE_NAV for menu rows; plain ACTIVATE raises window */
        fprintf(f, "ACTIVATE\n");
        fclose(f);
    }
}

static void jump_nav(int nav) {
    /* Match tab by INDEX (livedesk ledger index written by entity window) */
    for (int i = 0; i < g_ntabs; i++) {
        if (g_tabs[i].index == nav) {
            activate_tab(i);
            return;
        }
    }
    /* Also try 0-based tab slot */
    if (nav >= 0 && nav < g_ntabs) activate_tab(nav);
}

static void run_crypts(void) {
    char ps1[PATH_BUF];
    join2(ps1, sizeof(ps1), g_house, "$.crypts\\button.ps1");
    char cmd[PATH_BUF * 2];
    if (_access(ps1, 0) == 0)
        snprintf(cmd, sizeof(cmd), "powershell -ExecutionPolicy Bypass -File \"%s\" run", ps1);
    else {
        char sh[PATH_BUF];
        join2(sh, sizeof(sh), g_house, "$.crypts\\button.sh");
        snprintf(cmd, sizeof(cmd), "bash \"%s\" run", sh);
    }
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                   CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                   NULL, NULL, &si, &pi);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, POLL_MS, NULL);
        return 0;
    case WM_TIMER:
        if (wParam == 1) {
            reload_tabs();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        int x = LOWORD(lParam);
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        if (x >= screen_w - 36) { DestroyWindow(hwnd); return 0; }
        if (x >= screen_w - 72) { run_crypts(); return 0; }
        /* nav buffer zone (center) click focuses for typing */
        int mid0 = screen_w / 2 - 60;
        int mid1 = screen_w / 2 + 60;
        if (x >= mid0 && x < mid1) {
            SetFocus(hwnd);
            return 0;
        }
        int ti = x / TAB_W;
        if (ti >= 0 && ti < g_ntabs) activate_tab(ti);
        return 0;
    }
    case WM_CHAR: {
        if (wParam == 13 || wParam == 10) { /* Enter */
            if (g_nav_len > 0) {
                g_nav_buf[g_nav_len] = 0;
                jump_nav(atoi(g_nav_buf));
                g_nav_len = 0;
                g_nav_buf[0] = 0;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        if (wParam == 8) { /* backspace */
            if (g_nav_len > 0) g_nav_buf[--g_nav_len] = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == 27) { /* esc clear */
            g_nav_len = 0; g_nav_buf[0] = 0;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (isdigit((unsigned char)wParam) && g_nav_len < 8) {
            g_nav_buf[g_nav_len++] = (char)wParam;
            g_nav_buf[g_nav_len] = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(30, 30, 40));
        FillRect(hdc, &rc, br); DeleteObject(br);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(220, 220, 220));
        for (int i = 0; i < g_ntabs; i++) {
            RECT tr = { i * TAB_W + 2, 2, (i + 1) * TAB_W - 2, BAR_H - 2 };
            HBRUSH tb = CreateSolidBrush(RGB(60, 60, 90));
            FillRect(hdc, &tr, tb); DeleteObject(tb);
            char lab[160];
            snprintf(lab, sizeof(lab), "[%d] %s", g_tabs[i].index, g_tabs[i].entity);
            DrawTextA(hdc, lab, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        /* center Nav field (Linux middle terminal input) */
        int sw = rc.right;
        RECT r_nav = { sw / 2 - 50, 4, sw / 2 + 50, BAR_H - 4 };
        HBRUSH nb = CreateSolidBrush(RGB(20, 20, 30));
        FillRect(hdc, &r_nav, nb); DeleteObject(nb);
        char navlab[32];
        if (g_nav_len > 0) snprintf(navlab, sizeof(navlab), "Nav:%s", g_nav_buf);
        else snprintf(navlab, sizeof(navlab), "Nav:_");
        DrawTextA(hdc, navlab, -1, &r_nav, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT r_dollar = { sw - 72, 2, sw - 38, BAR_H - 2 };
        RECT r_x = { sw - 36, 2, sw - 4, BAR_H - 2 };
        DrawTextA(hdc, "$", -1, &r_dollar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextA(hdc, "X", -1, &r_x, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        remove(g_pidpath);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tp_taskbar <house_root>\n");
        return 1;
    }
    snprintf(g_house, sizeof(g_house), "%s", argv[1]);
    for (char *p = g_house; *p; p++) if (*p == '/') *p = '\\';
    if (!g_house[0] || strcmp(g_house, ".") == 0 || strcmp(g_house, ".\\") == 0)
        snprintf(g_house, sizeof(g_house), ".");

    join2(g_pidpath, sizeof(g_pidpath), g_house, "#.desktop\\livedesk_taskbar.pid");
    write_pidfile();
    reload_tabs();
    g_nav_len = 0; g_nav_buf[0] = 0;

    HINSTANCE hi = GetModuleHandle(NULL);
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "KHTPM_Taskbar";
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "KHTPM_Taskbar", "livedesk taskbar",
        WS_POPUP | WS_VISIBLE,
        0, sh - BAR_H, sw, BAR_H,
        NULL, NULL, hi, NULL);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
