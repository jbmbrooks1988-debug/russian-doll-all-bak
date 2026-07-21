/* userpal_menu_input - piece.pdl METHOD-table-driven ACTION dispatch
 * for user-pal's own (single-screen) login UI, modeled directly on
 * pal-forum's own forum_menu_input.c (PAL-FORUM-STANDARD.txt's own
 * proven ${piece_methods} + real <cli_io> pattern - reused, not
 * reinvented, per PAL-REFACTOR-STANDARD.txt's own preference order).
 *
 * user-pal has exactly one real screen (login.chtpm) - there is no
 * href navigation to derive "which screen is current" from, unlike
 * pal-forum/pal-chain. SIGNUP/LOGIN/LOGOUT all operate on the same
 * screen and just change last_message + current_login.txt.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_menu_input.+x <keycode> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 2048
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_MENU_ITEMS 32

typedef struct {
    char label[128];
    char command[512];
} MenuItem;

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv_str_local(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[32][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 32 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
        fclose(f);
    }
    size_t key_len = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < nlines; i++) {
        if (strncmp(lines[i], key, key_len) == 0 && lines[i][key_len] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else {
            fputs(lines[i], f);
        }
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';
    return s;
}

static int load_menu_items(MenuItem *items, int max_items) {
    char pdl_path[PATH_BUF];
    snprintf(pdl_path, sizeof(pdl_path), "%s/projects/user-pal/pieces/login/piece.pdl", project_root);
    FILE *f = fopen(pdl_path, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int n = 0;
    while (n < max_items && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *p2 = strchr(p1 + 1, '|');
        if (!p2) continue;
        *p2 = '\0';
        char *label = trim(p1 + 1);
        char *command = trim(p2 + 1);
        snprintf(items[n].label, sizeof(items[n].label), "%s", label);
        snprintf(items[n].command, sizeof(items[n].command), "%s", command);
        n++;
    }
    fclose(f);
    return n;
}

static void read_gui_state_str(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/user-pal/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(out, out_sz, "%s", v);
#pragma GCC diagnostic pop
        }
    }
    fclose(f);
}

/* Shells a command, captures its first stdout line as the message -
 * matches forum_menu_input.c's own RUN: capture shape exactly. */
static void run_capture(const char *cmd, char *message, size_t message_sz) {
    char full[PATH_BUF * 2];
    snprintf(full, sizeof(full), "cd '%s' && %s 2>&1", project_root, cmd);
    FILE *p = popen(full, "r");
    if (!p) { snprintf(message, message_sz, "Action failed to start."); return; }
    if (!fgets(message, message_sz, p)) snprintf(message, message_sz, "Ran: %s", cmd);
    else message[strcspn(message, "\n")] = '\0';
    pclose(p);
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    resolve_root();

    char state_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/system/userpal_menu_state.txt", project_root);

    int key = atoi(argv[1]);
    if (key == 0) return 0;  /* no href navigation in this project - nothing to catch up on */

    MenuItem items[MAX_MENU_ITEMS];
    int item_count = load_menu_items(items, MAX_MENU_ITEMS);

    int resolved_item = 0;
    if (key >= '0' && key <= '9') resolved_item = (key - '0') - 1;
    else if (key > 9 && key < 1000) resolved_item = key - 1;

    char message[MAX_LINE];
    read_kv_str_local(state_path, "last_message", message, sizeof(message));

    if (resolved_item >= 1 && resolved_item <= item_count) {
        const char *cmd = items[resolved_item - 1].command;

        if (strcmp(cmd, "SIGNUP") == 0) {
            char user_id[128], display_name[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            read_gui_state_str("display_name_input", display_name, sizeof(display_name));
            if (!user_id[0] || !display_name[0]) {
                snprintf(message, sizeof(message), "Enter a user ID and display name, then click Create Account.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_create_account.+x '%s' '%s'", user_id, display_name);
                run_capture(cmdbuf, message, sizeof(message));
                if (strstr(message, "created")) {
                    char loginbuf[PATH_BUF];
                    snprintf(loginbuf, sizeof(loginbuf), "./ops/+x/userpal_login.+x '%s'", user_id);
                    char discard[MAX_LINE];
                    run_capture(loginbuf, discard, sizeof(discard));
                    snprintf(message, sizeof(message), "Account created - logged in as %s.", user_id);
                }
            }
        } else if (strcmp(cmd, "LOGIN") == 0) {
            char user_id[128];
            read_gui_state_str("user_id_input", user_id, sizeof(user_id));
            if (!user_id[0]) {
                snprintf(message, sizeof(message), "Enter your user ID, then click Log In.");
            } else {
                char cmdbuf[PATH_BUF * 2];
                snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_login.+x '%s'", user_id);
                run_capture(cmdbuf, message, sizeof(message));
            }
        } else if (strcmp(cmd, "LOGOUT") == 0) {
            char cmdbuf[PATH_BUF];
            snprintf(cmdbuf, sizeof(cmdbuf), "./ops/+x/userpal_logout.+x");
            run_capture(cmdbuf, message, sizeof(message));
        } else {
            snprintf(message, sizeof(message), "Unknown command.");
        }
    }

    write_kv(state_path, "last_message", message);

    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/userpal_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }
    return 0;
}
