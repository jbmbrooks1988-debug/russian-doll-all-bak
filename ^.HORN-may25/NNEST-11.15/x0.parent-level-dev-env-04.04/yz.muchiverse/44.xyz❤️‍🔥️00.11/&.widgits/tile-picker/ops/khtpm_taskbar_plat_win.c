/* khtpm_taskbar_plat_win.c — Win draw/events only; logic = khtpm_taskbar_core */
#ifndef _WIN32
#error "Windows only"
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "khtpm_taskbar_core.h"
#include "khtpm_taskbar_plat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static KtbState *g_s;
static HWND g_hwnd;

void ktb_plat_run_command(const char *cmd, const char *house_root) {
    if (!cmd || !cmd[0]) return;
    char portable[KTB_PATH_BUF];
    ktb_action_portable(cmd, portable, sizeof(portable));
    char cmdline[KTB_PATH_BUF * 2];
    if (strstr(portable, "button.ps1") || strstr(portable, ".ps1")) {
        /* $.crypts/button.sh run → button.ps1 run on Win */
        if (strstr(portable, "button.sh")) {
            snprintf(cmdline, sizeof(cmdline),
                     "powershell -ExecutionPolicy Bypass -File \"$.crypts\\button.ps1\" run");
        } else {
            snprintf(cmdline, sizeof(cmdline),
                     "powershell -ExecutionPolicy Bypass -File \"%s\"", portable);
        }
    } else if (strstr(portable, ".sh")) {
        snprintf(cmdline, sizeof(cmdline), "bash \"%s\"", portable);
    } else if (strstr(portable, "button.sh")) {
        snprintf(cmdline, sizeof(cmdline),
                 "powershell -ExecutionPolicy Bypass -File \"$.crypts\\button.ps1\" run");
    } else {
        snprintf(cmdline, sizeof(cmdline), "%s", portable);
    }
    (void)house_root;
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                   CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                   NULL, NULL, &si, &pi);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 400, NULL);
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            ktb_reload(g_s);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lp);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int close_x0 = ktb_close_x0(sw);
        if (x >= close_x0) {
            ktb_quit_and_save(g_s);
            DestroyWindow(hwnd);
            return 0;
        }
        int si = ktb_shortcut_index_at_x(x, sw, g_s->n_shortcuts);
        if (si >= 0) {
            ktb_plat_run_command(g_s->shortcuts[si].command, g_s->house_root);
            return 0;
        }
        int tabs_right = ktb_shortcuts_x0(sw, g_s->n_shortcuts) - 4;
        int ti = ktb_tab_index_at_x(x, g_s->n_tabs, tabs_right);
        if (ti >= 0) ktb_activate_tab(g_s, ti);
        else SetFocus(hwnd);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_LEFT) { ktb_focus_delta(g_s, -1); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (wp == VK_RIGHT) { ktb_focus_delta(g_s, 1); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (wp == VK_RETURN) { ktb_digit_enter(g_s); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (wp == VK_ESCAPE) { ktb_digit_clear(g_s); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (wp == VK_BACK) { ktb_digit_backspace(g_s); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        return 0;
    case WM_CHAR:
        if (wp >= '0' && wp <= '9') {
            ktb_digit_push(g_s, (char)wp);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        /* theme: simple RGB map for common names / #rrggbb later */
        COLORREF bg = RGB(0, 0, 0), fg = RGB(0, 255, 0);
        if (strcmp(g_s->theme_bg, "white") == 0) bg = RGB(255, 255, 255);
        if (strcmp(g_s->theme_fg, "black") == 0) fg = RGB(0, 0, 0);
        if (g_s->theme_bg[0] == '#' && strlen(g_s->theme_bg) >= 7) {
            unsigned r, g, b;
            if (sscanf(g_s->theme_bg + 1, "%02x%02x%02x", &r, &g, &b) == 3)
                bg = RGB(r, g, b);
        }
        if (g_s->theme_fg[0] == '#' && strlen(g_s->theme_fg) >= 7) {
            unsigned r, g, b;
            if (sscanf(g_s->theme_fg + 1, "%02x%02x%02x", &r, &g, &b) == 3)
                fg = RGB(r, g, b);
        }
        /* Background fill; whole-window alpha (50%) applied via
         * WS_EX_LAYERED + SetLayeredWindowAttributes in ktb_plat_run. */
        HBRUSH br = CreateSolidBrush(bg);
        FillRect(hdc, &rc, br); DeleteObject(br);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        int sw = rc.right;
        int close_x0 = ktb_close_x0(sw);
        int shortcuts_x0 = ktb_shortcuts_x0(sw, g_s->n_shortcuts);
        int tabs_right = shortcuts_x0 - 4;
        for (int i = 0; i < g_s->n_tabs; i++) {
            int x0 = i * KTB_TAB_W;
            if (x0 + 8 >= tabs_right) break;
            RECT tr = { x0 + 2, 2, x0 + KTB_TAB_W - 2, KTB_BAR_H - 2 };
            const char *cur = (i == g_s->tab_focus_idx) ? "[>]" : "[ ]";
            char lab[192];
            snprintf(lab, sizeof(lab), "%s %d. %s", cur, g_s->tabs[i].nav, g_s->tabs[i].entity);
            DrawTextA(hdc, lab, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        /* nav armed indicator */
        if (g_s->nav_armed || g_s->digit_len > 0) {
            RECT nr = { tabs_right - 90, 4, tabs_right - 4, KTB_BAR_H - 4 };
            char arm[32];
            snprintf(arm, sizeof(arm), "[%s]", g_s->digit_len ? g_s->digit_buf : "NAV");
            DrawTextA(hdc, arm, -1, &nr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        for (int i = 0; i < g_s->n_shortcuts; i++) {
            int sx0 = close_x0 - (i + 1) * KTB_SHORTCUT_W;
            RECT sr = { sx0, 2, sx0 + KTB_SHORTCUT_W - 4, KTB_BAR_H - 2 };
            DrawTextA(hdc, g_s->shortcuts[i].glyph, -1, &sr,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        RECT xr = { close_x0, 2, sw - 4, KTB_BAR_H - 2 };
        DrawTextA(hdc, "X", -1, &xr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int ktb_plat_run(KtbState *s) {
    g_s = s;
    HINSTANCE hi = GetModuleHandle(NULL);
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "KHTPM_TaskbarCore";
    RegisterClassA(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    /* Windows OS taskbar owns the bottom — put livedesk bar at TOP.
     * WS_EX_LAYERED + 50% alpha so the bar is translucent over the desk. */
    g_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        "KHTPM_TaskbarCore", "livedesk taskbar",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, sw, KTB_BAR_H,
        NULL, NULL, hi, NULL);
    if (!g_hwnd) return 1;
    /* 128/255 ≈ 50% opacity (background + glyphs share window alpha) */
    SetLayeredWindowAttributes(g_hwnd, 0, 128, LWA_ALPHA);
    ShowWindow(g_hwnd, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
