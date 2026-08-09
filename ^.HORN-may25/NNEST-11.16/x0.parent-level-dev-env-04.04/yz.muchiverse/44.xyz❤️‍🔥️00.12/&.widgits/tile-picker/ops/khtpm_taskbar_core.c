/* khtpm_taskbar_core.c — single toolbar logic for all platforms. */
#include "khtpm_taskbar_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <io.h>
static FILE *ktb_fopen(const char *path, const char *mode) {
    wchar_t wp[KTB_PATH_BUF], wm[16];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, KTB_PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, path, -1, wp, KTB_PATH_BUF))
        return fopen(path, mode);
    MultiByteToWideChar(CP_ACP, 0, mode, -1, wm, 16);
    return _wfopen(wp, wm);
}
#else
#  include <unistd.h>
#  include <signal.h>
#  define ktb_fopen fopen
#endif

static void path_join(char *out, size_t n, const char *a, const char *b) {
    if (!a || !a[0] || strcmp(a, ".") == 0) {
        snprintf(out, n, "%s", b ? b : "");
        return;
    }
    size_t al = strlen(a);
    if (a[al - 1] == '/' || a[al - 1] == '\\')
        snprintf(out, n, "%s%s", a, b);
    else
#ifdef _WIN32
        snprintf(out, n, "%s\\%s", a, b);
#else
        snprintf(out, n, "%s/%s", a, b);
#endif
}

int ktb_pid_alive(int pid) {
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    DWORD code = 0;
    int ok = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return ok;
#else
    return kill((pid_t)pid, 0) == 0 || errno != ESRCH;
#endif
}

void ktb_init(KtbState *s, const char *house_root) {
    memset(s, 0, sizeof(*s));
    snprintf(s->house_root, sizeof(s->house_root), "%s",
             (house_root && house_root[0]) ? house_root : ".");
    path_join(s->pid_path, sizeof(s->pid_path), s->house_root, "#.desktop/livedesk_taskbar.pid");
#ifdef _WIN32
    for (char *p = s->pid_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    snprintf(s->theme_bg, sizeof(s->theme_bg), "white");
    snprintf(s->theme_fg, sizeof(s->theme_fg), "black");
    s->tab_focus_idx = 0;
}

void ktb_write_pidfile(KtbState *s, int pid) {
    FILE *f = ktb_fopen(s->pid_path, "w");
    if (f) { fprintf(f, "%d\n", pid); fclose(f); }
}

void ktb_unlink_pidfile(const KtbState *s) {
    remove(s->pid_path);
}

static int load_shortcuts(KtbState *s) {
    char path[KTB_PATH_BUF];
    path_join(path, sizeof(path), s->house_root, "#.desktop/livedesk_shortcuts.pdl");
#ifdef _WIN32
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(path, "r");
    s->n_shortcuts = 0;
    if (!f) return 0;
    char line[KTB_PATH_BUF];
    while (s->n_shortcuts < KTB_MAX_SHORTCUTS && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SHORTCUT", 8) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *ge = end;
        while (ge > p && ge[-1] == ' ') ge--;
        size_t glen = (size_t)(ge - p);
        if (glen == 0 || glen >= sizeof(s->shortcuts[0].glyph)) continue;
        memcpy(s->shortcuts[s->n_shortcuts].glyph, p, glen);
        s->shortcuts[s->n_shortcuts].glyph[glen] = 0;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = 0;
        snprintf(s->shortcuts[s->n_shortcuts].command,
                 sizeof(s->shortcuts[0].command), "%s", v);
        s->n_shortcuts++;
    }
    fclose(f);
    return s->n_shortcuts;
}

static void load_theme(KtbState *s) {
    snprintf(s->theme_bg, sizeof(s->theme_bg), "white");
    snprintf(s->theme_fg, sizeof(s->theme_fg), "black");
    char path[KTB_PATH_BUF];
    path_join(path, sizeof(path), s->house_root, "#.desktop/livedesk_theme.pdl");
#ifdef _WIN32
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(path, "r");
    if (!f) return;
    char line[KTB_PATH_BUF];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "COLOR", 5) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char key[16];
        size_t klen = (size_t)(end - p);
        while (klen && p[klen - 1] == ' ') klen--;
        if (klen >= sizeof(key)) continue;
        memcpy(key, p, klen); key[klen] = 0;
        char *v = end + 1;
        while (*v == ' ') v++;
        v[strcspn(v, "\r\n")] = 0;
        if (strcmp(key, "bg") == 0) snprintf(s->theme_bg, sizeof(s->theme_bg), "%s", v);
        else if (strcmp(key, "fg") == 0) snprintf(s->theme_fg, sizeof(s->theme_fg), "%s", v);
    }
    fclose(f);
}

/* load_tabs + prune dead (Linux design) */
static int load_tabs(KtbState *s) {
    char reg_path[KTB_PATH_BUF], tmp_path[KTB_PATH_BUF];
    path_join(reg_path, sizeof(reg_path), s->house_root, "#.desktop/livedesk_open.txt");
    path_join(tmp_path, sizeof(tmp_path), s->house_root, "#.desktop/livedesk_open.txt.tmp");
#ifdef _WIN32
    for (char *p = reg_path; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(reg_path, "r");
    s->n_tabs = 0;
    if (!f) return 0;
    FILE *w = ktb_fopen(tmp_path, "w");
    char line[KTB_PATH_BUF];
    while (s->n_tabs < KTB_MAX_TABS && fgets(line, sizeof(line), f)) {
        KtbTab t;
        memset(&t, 0, sizeof(t));
        char *p;
        if ((p = strstr(line, "PID="))) t.pid = atoi(p + 4);
        if ((p = strstr(line, "ENTITY="))) {
            char *e = p + 7;
            char *end = strchr(e, '|');
            size_t len = end ? (size_t)(end - e) : strcspn(e, "\r\n");
            if (len >= sizeof(t.entity)) len = sizeof(t.entity) - 1;
            memcpy(t.entity, e, len);
            t.entity[len] = 0;
        }
        if ((p = strstr(line, "PATH="))) {
            snprintf(t.path, sizeof(t.path), "%s", p + 5);
            t.path[strcspn(t.path, "\r\n")] = 0;
        }
        if (!t.entity[0] || !ktb_pid_alive(t.pid)) continue;
        if (w) fputs(line, w);
        s->tabs[s->n_tabs++] = t;
    }
    fclose(f);
    if (w) { fclose(w); remove(reg_path); rename(tmp_path, reg_path); }
    return s->n_tabs;
}

static int nav_idx_of_pid(KtbTab *tabs, int n, int pid) {
    for (int i = 0; i < n; i++) if (tabs[i].pid == pid) return i;
    return -1;
}

/* sync_tab_claims — Linux design, KIND=tab only; leave KIND=row alone */
static void sync_tab_claims(KtbState *s) {
    char claims_path[KTB_PATH_BUF], tmp_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk_nav_claims.txt");
    path_join(tmp_path, sizeof(tmp_path), s->house_root, "#.desktop/livedesk_nav_claims.txt.tmp");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    for (int i = 0; i < s->n_tabs; i++) s->tabs[i].nav = 0;
    int max_nav = 0;
    FILE *w = ktb_fopen(tmp_path, "w");
    if (!w) return;
    FILE *f = ktb_fopen(claims_path, "r");
    if (f) {
        char line[KTB_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            char *navp = strstr(line, "NAV=");
            int nav_v = navp ? atoi(navp + 4) : 0;
            if (nav_v > max_nav) max_nav = nav_v;
            if (strncmp(line, "KIND=tab", 8) == 0) {
                char *pidp = strstr(line, "PID=");
                int pid_v = pidp ? atoi(pidp + 4) : -1;
                int idx = nav_idx_of_pid(s->tabs, s->n_tabs, pid_v);
                if (idx >= 0) {
                    s->tabs[idx].nav = nav_v;
                    fputs(line, w);
                }
            } else {
                fputs(line, w); /* keep KIND=row etc. */
            }
        }
        fclose(f);
    }
    for (int i = 0; i < s->n_tabs; i++) {
        if (s->tabs[i].nav == 0) {
            max_nav++;
            s->tabs[i].nav = max_nav;
            fprintf(w, "KIND=tab|PID=%d|NAV=%d|ENTITY=%s|PATH=%s\n",
                    s->tabs[i].pid, s->tabs[i].nav, s->tabs[i].entity, s->tabs[i].path);
        }
    }
    fclose(w);
    remove(claims_path);
    rename(tmp_path, claims_path);
}

void ktb_reload(KtbState *s) {
    load_tabs(s);
    sync_tab_claims(s);
    load_shortcuts(s);
    load_theme(s);
    if (s->tab_focus_idx >= s->n_tabs) s->tab_focus_idx = s->n_tabs > 0 ? s->n_tabs - 1 : 0;
}

static void write_relay(const char *package_path, const char *cmd) {
    char relay[KTB_PATH_BUF];
    path_join(relay, sizeof(relay), package_path, "interact_relay.txt");
#ifdef _WIN32
    for (char *p = relay; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(relay, "w");
    if (f) { fprintf(f, "%s\n", cmd); fclose(f); }
}

void ktb_activate_tab(KtbState *s, int idx) {
    if (idx < 0 || idx >= s->n_tabs) return;
    /* Linux: raise + OPEN_CONTEXT (same as right-click on tile) */
    write_relay(s->tabs[idx].path, "ACTIVATE");
    write_relay(s->tabs[idx].path, "OPEN_CONTEXT");
    /* second write overwrites — send both lines in one write */
    char relay[KTB_PATH_BUF];
    path_join(relay, sizeof(relay), s->tabs[idx].path, "interact_relay.txt");
#ifdef _WIN32
    for (char *p = relay; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(relay, "w");
    if (f) {
        fprintf(f, "ACTIVATE\nOPEN_CONTEXT\n");
        fclose(f);
    }
    s->tab_focus_idx = idx;
}

void ktb_jump_nav(KtbState *s, int nav_n) {
    char claims_path[KTB_PATH_BUF];
    path_join(claims_path, sizeof(claims_path), s->house_root, "#.desktop/livedesk_nav_claims.txt");
#ifdef _WIN32
    for (char *p = claims_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    FILE *f = ktb_fopen(claims_path, "r");
    if (!f) {
        /* fallback: match tab.nav */
        for (int i = 0; i < s->n_tabs; i++)
            if (s->tabs[i].nav == nav_n) { ktb_activate_tab(s, i); return; }
        return;
    }
    char line[KTB_PATH_BUF];
    int found = 0;
    char kind[16] = "", path[KTB_PATH_BUF] = "";
    while (fgets(line, sizeof(line), f)) {
        char *navp = strstr(line, "NAV=");
        if (!navp || atoi(navp + 4) != nav_n) continue;
        snprintf(kind, sizeof(kind), "%s",
                 strncmp(line, "KIND=tab", 8) == 0 ? "tab" : "row");
        char *pp = strstr(line, "PATH=");
        if (pp) {
            snprintf(path, sizeof(path), "%s", pp + 5);
            path[strcspn(path, "\r\n")] = 0;
        }
        found = 1;
        break;
    }
    fclose(f);
    if (!found) return;
    if (strcmp(kind, "tab") == 0) {
        for (int i = 0; i < s->n_tabs; i++)
            if (s->tabs[i].nav == nav_n) { ktb_activate_tab(s, i); return; }
    } else {
        /* menu row: inject ACTIVATE_NAV into that package */
        char relay[KTB_PATH_BUF];
        path_join(relay, sizeof(relay), path, "interact_relay.txt");
#ifdef _WIN32
        for (char *p = relay; *p; p++) if (*p == '/') *p = '\\';
#endif
        FILE *rf = ktb_fopen(relay, "w");
        if (rf) {
            fprintf(rf, "ACTIVATE_NAV:%d\n", nav_n);
            fclose(rf);
        }
    }
}

void ktb_digit_clear(KtbState *s) {
    s->digit_len = 0;
    s->digit_buf[0] = 0;
    s->nav_armed = 0;
}

void ktb_digit_push(KtbState *s, char c) {
    if (!isdigit((unsigned char)c) || s->digit_len >= 8) return;
    s->digit_buf[s->digit_len++] = c;
    s->digit_buf[s->digit_len] = 0;
    s->nav_armed = 1;
}

void ktb_digit_backspace(KtbState *s) {
    if (s->digit_len > 0) s->digit_buf[--s->digit_len] = 0;
    if (s->digit_len == 0) s->nav_armed = 0;
}

void ktb_digit_enter(KtbState *s) {
    if (s->digit_len <= 0) {
        /* Enter with empty buffer: activate focused tab */
        ktb_activate_tab(s, s->tab_focus_idx);
        return;
    }
    ktb_jump_nav(s, atoi(s->digit_buf));
    ktb_digit_clear(s);
}

void ktb_focus_delta(KtbState *s, int delta) {
    if (s->n_tabs <= 0) return;
    s->tab_focus_idx += delta;
    if (s->tab_focus_idx < 0) s->tab_focus_idx = s->n_tabs - 1;
    if (s->tab_focus_idx >= s->n_tabs) s->tab_focus_idx = 0;
    s->nav_armed = 1;
}

void ktb_action_portable(const char *in, char *out, size_t out_sz) {
    if (!in) { out[0] = 0; return; }
    const char *markers[] = {
        "/$.crypts/", "\\$.crypts\\",
        "/&.widgits/", "\\&.widgits\\",
        "/@.apps/", "\\@.apps\\",
        NULL
    };
    for (int i = 0; markers[i]; i++) {
        const char *m = strstr(in, markers[i]);
        if (m) { snprintf(out, out_sz, "%s", m + 1); return; }
    }
    snprintf(out, out_sz, "%s", in);
}

void ktb_quit_and_save(KtbState *s) {
    /* CLOSE all entities */
    for (int i = 0; i < s->n_tabs; i++)
        write_relay(s->tabs[i].path, "CLOSE");

    char pdl_path[KTB_PATH_BUF], tmp_path[KTB_PATH_BUF];
    path_join(pdl_path, sizeof(pdl_path), s->house_root, "$.crypts/autostart.pdl");
    path_join(tmp_path, sizeof(tmp_path), s->house_root, "$.crypts/autostart.pdl.tmp");
#ifdef _WIN32
    for (char *p = pdl_path; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = tmp_path; *p; p++) if (*p == '/') *p = '\\';
#endif
    char keep[64][KTB_PATH_BUF];
    int n_keep = 0;
    FILE *rf = ktb_fopen(pdl_path, "r");
    if (rf) {
        char line[KTB_PATH_BUF];
        while (n_keep < 64 && fgets(line, sizeof(line), rf)) {
            if (strncmp(line, "LAUNCH", 6) == 0) continue;
            snprintf(keep[n_keep], sizeof(keep[0]), "%s", line);
            n_keep++;
        }
        fclose(rf);
    }
    FILE *wf = ktb_fopen(tmp_path, "w");
    if (wf) {
        for (int i = 0; i < n_keep; i++) fputs(keep[i], wf);
        /* relative LAUNCH rows only (portable) */
        fprintf(wf, "LAUNCH       | tool-bar             | '&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x' '.'\n");
        for (int i = 0; i < s->n_tabs; i++) {
            fprintf(wf, "LAUNCH       | %-20s | '&.widgits/tile-picker/ops/+x/tp_desktop_window.+x' '%s'\n",
                    s->tabs[i].entity, s->tabs[i].path);
        }
        fclose(wf);
        remove(pdl_path);
        rename(tmp_path, pdl_path);
    }
    ktb_unlink_pidfile(s);
}

int ktb_close_x0(int screen_w) {
    return screen_w - KTB_CLOSE_W;
}

int ktb_shortcuts_x0(int screen_w, int n_shortcuts) {
    return ktb_close_x0(screen_w) - n_shortcuts * KTB_SHORTCUT_W;
}

int ktb_tab_index_at_x(int x, int n_tabs, int tabs_right) {
    if (x < 0 || x >= tabs_right) return -1;
    int i = x / KTB_TAB_W;
    if (i < 0 || i >= n_tabs) return -1;
    return i;
}

int ktb_shortcut_index_at_x(int x, int screen_w, int n_shortcuts) {
    int close_x0 = ktb_close_x0(screen_w);
    for (int i = 0; i < n_shortcuts; i++) {
        int sx0 = close_x0 - (i + 1) * KTB_SHORTCUT_W;
        if (x >= sx0 && x < sx0 + KTB_SHORTCUT_W) return i;
    }
    return -1;
}
