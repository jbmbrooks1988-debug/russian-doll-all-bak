/* khtpm_taskbar_core.h — SHARED toolbar/taskbar design logic.
 * ONE logic set for Linux + Windows (WIN-COMPAT-RULE). Plat only draws.
 */
#ifndef KHTPM_TASKBAR_CORE_H
#define KHTPM_TASKBAR_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KTB_PATH_BUF 4352
#define KTB_MAX_TABS 64
#define KTB_MAX_SHORTCUTS 16
#define KTB_BAR_H 36
#define KTB_TAB_W 160
#define KTB_CLOSE_W 36
#define KTB_SHORTCUT_W 32

typedef struct {
    int pid;
    int nav;                 /* shared live nav number */
    char entity[128];
    char path[KTB_PATH_BUF];
} KtbTab;

typedef struct {
    char glyph[16];
    char command[KTB_PATH_BUF];
} KtbShortcut;

typedef struct {
    char house_root[KTB_PATH_BUF];
    char pid_path[KTB_PATH_BUF];
    KtbTab tabs[KTB_MAX_TABS];
    int n_tabs;
    KtbShortcut shortcuts[KTB_MAX_SHORTCUTS];
    int n_shortcuts;
    char theme_bg[32];
    char theme_fg[32];
    /* Nav digit buffer (terminal-style middle input) */
    char digit_buf[16];
    int digit_len;
    int tab_focus_idx; /* keyboard cursor among tabs */
    int nav_armed;
} KtbState;

void ktb_init(KtbState *s, const char *house_root);
void ktb_write_pidfile(KtbState *s, int pid);
void ktb_unlink_pidfile(const KtbState *s);

/* Reload tabs from livedesk_open (prune dead), sync nav claims, shortcuts, theme */
void ktb_reload(KtbState *s);

/* Activate tab by index: write ACTIVATE + OPEN_CONTEXT to interact_relay */
void ktb_activate_tab(KtbState *s, int idx);

/* Jump by shared NAV number: tab raise/open menu, or ACTIVATE_NAV to package */
void ktb_jump_nav(KtbState *s, int nav_n);

/* Digit buffer */
void ktb_digit_clear(KtbState *s);
void ktb_digit_push(KtbState *s, char c);
void ktb_digit_backspace(KtbState *s);
void ktb_digit_enter(KtbState *s); /* parse buffer → jump_nav */

/* Focus move among tabs */
void ktb_focus_delta(KtbState *s, int delta);

/* Quit+save: rewrite autostart LAUNCH rows from open tabs (portable paths) */
void ktb_quit_and_save(KtbState *s);

/* Layout helpers for plat drawing */
int ktb_close_x0(int screen_w);
int ktb_shortcuts_x0(int screen_w, int n_shortcuts);
int ktb_tab_index_at_x(int x, int n_tabs, int tabs_right);
int ktb_shortcut_index_at_x(int x, int screen_w, int n_shortcuts);

/* Portable path strip for shortcut commands */
void ktb_action_portable(const char *in, char *out, size_t out_sz);

int ktb_pid_alive(int pid);

#ifdef __cplusplus
}
#endif
#endif
