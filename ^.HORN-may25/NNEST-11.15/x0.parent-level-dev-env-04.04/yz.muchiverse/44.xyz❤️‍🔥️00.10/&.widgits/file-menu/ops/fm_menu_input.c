#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 4096
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

#define ARROW_UP    1002
#define ARROW_DOWN  1003

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_kv(const char *path, const char *key, const char *val) {
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *in = fopen(path, "r");
    FILE *out = fopen(tmp, "w");
    if (!out) return;
    int found = 0;
    if (in) {
        char line[MAX_LINE];
        size_t klen = strlen(key);
        while (fgets(line, sizeof(line), in)) {
            if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
                fprintf(out, "%s=%s\n", key, val);
                found = 1;
            } else {
                fputs(line, out);
            }
        }
        fclose(in);
    }
    if (!found) fprintf(out, "%s=%s\n", key, val);
    fclose(out);
    rename(tmp, path);
}

static int has_focus(char *session_root, size_t sr_sz) {
    char focus[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/pieces/system/focus.txt", project_root);
    if (access(focus, F_OK) != 0) return 0;
    char inbox[MAX_PATH] = "";
    read_kv(focus, "inbox_path", inbox, sizeof(inbox));
    read_kv(focus, "session_root", session_root, sr_sz);
    if (!inbox[0] || !session_root[0]) return 0;
    if (access(session_root, F_OK) != 0) return 0;
    return 1;
}

static void enqueue_cmd(const char *cmd) {
    char cmd_path[PATH_BUF];
    snprintf(cmd_path, sizeof(cmd_path), "%s/ops/+x/fm_enqueue_cmd.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = { cmd_path, project_root, (char *)cmd, NULL };
        execvp(args[0], args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

static void enqueue_cmd_with_path(const char *cmd, const char *path_arg) {
    char cmd_path[PATH_BUF];
    snprintf(cmd_path, sizeof(cmd_path), "%s/ops/+x/fm_enqueue_cmd.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = { cmd_path, project_root, (char *)cmd, (char *)path_arg, NULL };
        execvp(args[0], args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

static void set_focus(const char *session_root_target) {
    char focus_path[PATH_BUF];
    snprintf(focus_path, sizeof(focus_path), "%s/ops/+x/fm_set_focus.+x", project_root);
    pid_t pid = fork();
    if (pid == 0) {
        char wdir[PATH_BUF];
        snprintf(wdir, sizeof(wdir), "%s/pieces/system", project_root);
        char *args[] = { focus_path, wdir, (char *)session_root_target, NULL };
        execvp(args[0], args);
        _exit(1);
    }
    waitpid(pid, NULL, 0);
}

static int scan_entry_count(const char *dir, const char *filter) {
    char cmd[MAX_PATH + 64];
    if (filter[0])
        snprintf(cmd, sizeof(cmd), "%s/ops/+x/fm_scan_dir.+x \"%s\" \"%s\" 2>/dev/null | wc -l",
                 project_root, dir, filter);
    else
        snprintf(cmd, sizeof(cmd), "%s/ops/+x/fm_scan_dir.+x \"%s\" 2>/dev/null | wc -l",
                 project_root, dir);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char buf[64];
    if (!fgets(buf, sizeof(buf), p)) { pclose(p); return 0; }
    pclose(p);
    return atoi(buf);
}

static void read_entry_at_index(const char *dir, const char *filter, int idx,
                                 char *name, size_t name_sz, int *is_dir) {
    name[0] = '\0';
    *is_dir = 0;
    char cmd[MAX_PATH + 64];
    if (filter[0])
        snprintf(cmd, sizeof(cmd), "%s/ops/+x/fm_scan_dir.+x \"%s\" \"%s\" 2>/dev/null",
                 project_root, dir, filter);
    else
        snprintf(cmd, sizeof(cmd), "%s/ops/+x/fm_scan_dir.+x \"%s\" 2>/dev/null",
                 project_root, dir);
    FILE *p = popen(cmd, "r");
    if (!p) return;
    char line[MAX_LINE];
    int i = 0;
    while (fgets(line, sizeof(line), p)) {
        if (i == idx) {
            char kind[16] = "";
            sscanf(line, "%15[^|]|%511[^|]", kind, name);
            *is_dir = (strcmp(kind, "DIR") == 0);
            break;
        }
        i++;
    }
    pclose(p);
}

static void execute_menu_option(const char *fm_path, int option) {
    char session_root[MAX_PATH] = "";
    int focused = has_focus(session_root, sizeof(session_root));

    if (option == 1) {
        if (focused) {
            enqueue_cmd("NEW");
            write_kv(fm_path, "status_line", "NEW sent");
        } else {
            write_kv(fm_path, "status_line", "No focus: cannot NEW");
        }
    } else if (option == 2) {
        if (focused) {
            enqueue_cmd("SAVE");
            write_kv(fm_path, "status_line", "SAVE sent");
        } else {
            write_kv(fm_path, "status_line", "No focus: cannot SAVE");
        }
    } else if (option == 3) {
        if (focused) {
            write_kv(fm_path, "mode", "file_browser");
            write_kv(fm_path, "browse_mode", "save_as");
            write_kv(fm_path, "browse_dir", session_root);
            write_kv(fm_path, "cursor_pos", "0");
            write_kv(fm_path, "search_text", "");
            write_kv(fm_path, "path_buffer", "");
            write_kv(fm_path, "status_line", "Type path or browse to a file");
        } else {
            write_kv(fm_path, "status_line", "No focus: cannot SAVE AS");
        }
    } else if (option == 4) {
        if (focused) {
            write_kv(fm_path, "mode", "file_browser");
            write_kv(fm_path, "browse_mode", "load");
            write_kv(fm_path, "browse_dir", session_root);
            write_kv(fm_path, "cursor_pos", "0");
            write_kv(fm_path, "search_text", "");
            write_kv(fm_path, "path_buffer", "");
            write_kv(fm_path, "status_line", "Browse to a file or type path");
        } else {
            write_kv(fm_path, "status_line", "No focus: cannot LOAD");
        }
    } else if (option == 5) {
        write_kv(fm_path, "mode", "peer_list");
        write_kv(fm_path, "cursor_pos", "0");
        write_kv(fm_path, "status_line", "Select a project to pair with");
    } else if (option == 6) {
        char pair_dir[PATH_BUF] = "/";
        const char *home = getenv("HOME");
        if (home) snprintf(pair_dir, sizeof(pair_dir), "%s", home);
        write_kv(fm_path, "mode", "file_browser");
        write_kv(fm_path, "browse_mode", "pair");
        write_kv(fm_path, "browse_dir", pair_dir);
        write_kv(fm_path, "cursor_pos", "0");
        write_kv(fm_path, "search_text", "");
        write_kv(fm_path, "path_buffer", "");
        write_kv(fm_path, "status_line", "Navigate to a project session dir");
    }
}

static void handle_main_menu(const char *fm_path, int key) {
    char cursor_str[64];
    int cursor_pos = 0;
    read_kv(fm_path, "cursor_pos", cursor_str, sizeof(cursor_str));
    if (cursor_str[0]) cursor_pos = atoi(cursor_str);

    if (key == 27) {
        /* ESC: do nothing in main menu */
        return;
    }

    if (key == ARROW_UP || key == ARROW_DOWN) {
        /* Up/Down arrows */
        if (key == ARROW_UP) {
            if (cursor_pos > 1) cursor_pos--;
        } else {
            if (cursor_pos < 6) cursor_pos++;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", cursor_pos);
        write_kv(fm_path, "cursor_pos", buf);
        return;
    }

    if (key == 10 || key == 13) {
        /* Enter: execute option at cursor position */
        if (cursor_pos >= 1 && cursor_pos <= 6) {
            execute_menu_option(fm_path, cursor_pos);
        }
        return;
    }

    if (key >= 49 && key <= 54) {
        /* Direct numeric key: 1-6 (ASCII 49-54) */
        int option = key - 48;
        cursor_pos = option;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", cursor_pos);
        write_kv(fm_path, "cursor_pos", buf);
        execute_menu_option(fm_path, option);
        return;
    }
}

static void handle_file_browser(const char *fm_path, int key) {
    char browse_mode[64], browse_dir[PATH_BUF], path_buffer[MAX_LINE];
    char search_text[MAX_LINE], cursor_str[64];
    int cursor_pos = 0;
    read_kv(fm_path, "browse_mode", browse_mode, sizeof(browse_mode));
    read_kv(fm_path, "browse_dir", browse_dir, sizeof(browse_dir));
    read_kv(fm_path, "path_buffer", path_buffer, sizeof(path_buffer));
    read_kv(fm_path, "search_text", search_text, sizeof(search_text));
    read_kv(fm_path, "cursor_pos", cursor_str, sizeof(cursor_str));
    if (cursor_str[0]) cursor_pos = atoi(cursor_str);

    int entry_count = 0;
    if (browse_dir[0])
        entry_count = scan_entry_count(browse_dir, search_text);

    /* Layout indices:
     * 0 = SEARCH field
     * 1 = FILE field
     * 2 = BACK
     * 3 .. 2+entry_count = directory entries
     * 2+entry_count+1 = confirm button
     * 2+entry_count+2 = cancel button
     */
    int back_idx = 2;
    int first_entry = 3;
    int confirm_idx = first_entry + entry_count;
    int cancel_idx = confirm_idx + 1;
    int max_idx = cancel_idx;

    if (key == 27) {
        write_kv(fm_path, "mode", "main_menu");
        write_kv(fm_path, "status_line", "");
        return;
    }

    if (key == 10 || key == 13) {
        if (cursor_pos == 0) {
            /* SEARCH field selected; typing goes to search_text */
        } else if (cursor_pos == 1) {
            /* FILE field selected; typing goes to path_buffer */
        } else if (cursor_pos == back_idx) {
            /* BACK: go to parent */
            char *last = strrchr(browse_dir, '/');
            if (last && last != browse_dir) {
                *last = '\0';
                write_kv(fm_path, "browse_dir", browse_dir);
            } else if (last == browse_dir) {
                browse_dir[1] = '\0';
                write_kv(fm_path, "browse_dir", "/");
            }
            write_kv(fm_path, "cursor_pos", "0");
            return;
        } else if (cursor_pos >= first_entry && cursor_pos < confirm_idx) {
            int entry_idx = cursor_pos - first_entry;
            char name[512] = "";
            int is_dir = 0;
            read_entry_at_index(browse_dir, search_text, entry_idx, name, sizeof(name), &is_dir);
            if (name[0]) {
                if (is_dir) {
                    char new_dir[PATH_BUF];
                    snprintf(new_dir, sizeof(new_dir), "%s/%s", browse_dir, name);
                    write_kv(fm_path, "browse_dir", new_dir);
                    write_kv(fm_path, "cursor_pos", "0");
                } else {
                    char full_path[PATH_BUF];
                    snprintf(full_path, sizeof(full_path), "%s/%s", browse_dir, name);
                    write_kv(fm_path, "path_buffer", full_path);
                    write_kv(fm_path, "cursor_pos", "1");
                    if (strcmp(browse_mode, "pair") == 0) {
                        char sr_check[MAX_PATH];
                        snprintf(sr_check, sizeof(sr_check), "%s/pieces/system/focus.txt", full_path);
                        if (access(full_path, F_OK) == 0) {
                            set_focus(full_path);
                            write_kv(fm_path, "mode", "main_menu");
                            write_kv(fm_path, "status_line", "Paired!");
                        } else {
                            write_kv(fm_path, "browse_dir", full_path);
                            write_kv(fm_path, "status_line", "Not a session root; try parent");
                        }
                    }
                }
            }
            return;
        } else if (cursor_pos == confirm_idx) {
            if (strcmp(browse_mode, "pair") == 0) {
                if (path_buffer[0]) {
                    set_focus(path_buffer);
                    write_kv(fm_path, "mode", "main_menu");
                    write_kv(fm_path, "status_line", "Paired!");
                } else {
                    write_kv(fm_path, "status_line", "No path selected for pairing");
                }
            } else if (path_buffer[0]) {
                const char *prefix = (strcmp(browse_mode, "load") == 0) ? "LOAD" : "SAVE_AS";
                enqueue_cmd_with_path(prefix, path_buffer);
                write_kv(fm_path, "mode", "main_menu");
                write_kv(fm_path, "path_buffer", "");
                write_kv(fm_path, "status_line", "Command sent");
            } else {
                write_kv(fm_path, "status_line", "No path selected");
            }
            return;
        } else if (cursor_pos == cancel_idx) {
            write_kv(fm_path, "mode", "main_menu");
            write_kv(fm_path, "status_line", "");
            return;
        }
        return;
    }

    if (key == ARROW_UP || key == ARROW_DOWN) {
        /* Up/Down arrows */
        if (key == ARROW_UP) {
            if (cursor_pos > 0) cursor_pos--;
        } else {
            if (cursor_pos < max_idx) cursor_pos++;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", cursor_pos);
        write_kv(fm_path, "cursor_pos", buf);
        return;
    }

    if (key == 127 || key == 8) {
        if (cursor_pos == 0) {
            size_t len = strlen(search_text);
            if (len > 0) { search_text[len - 1] = '\0'; write_kv(fm_path, "search_text", search_text); }
        } else if (cursor_pos == 1) {
            size_t len = strlen(path_buffer);
            if (len > 0) { path_buffer[len - 1] = '\0'; write_kv(fm_path, "path_buffer", path_buffer); }
        }
        return;
    }

    if (key >= 32 && key <= 126) {
        char ch = (char)key;
        if (cursor_pos == 0) {
            size_t len = strlen(search_text);
            if (len + 2 < sizeof(search_text)) {
                search_text[len] = ch;
                search_text[len + 1] = '\0';
                write_kv(fm_path, "search_text", search_text);
                write_kv(fm_path, "cursor_pos", "0");
            }
        } else if (cursor_pos == 1) {
            size_t len = strlen(path_buffer);
            if (len + 2 < sizeof(path_buffer)) {
                path_buffer[len] = ch;
                path_buffer[len + 1] = '\0';
                write_kv(fm_path, "path_buffer", path_buffer);
            }
        }
        return;
    }
}

static void handle_peer_list(const char *fm_path, int key) {
    char cursor_str[64];
    int cursor_pos = 0;
    read_kv(fm_path, "cursor_pos", cursor_str, sizeof(cursor_str));
    if (cursor_str[0]) cursor_pos = atoi(cursor_str);

    if (key == 27) {
        write_kv(fm_path, "mode", "main_menu");
        write_kv(fm_path, "status_line", "");
        return;
    }

    /* Count peers */
    char count_cmd[MAX_PATH + 64];
    snprintf(count_cmd, sizeof(count_cmd),
             "(%s/ops/+x/ledger_peers.+x editor 2>/dev/null; "
             "%s/ops/+x/ledger_peers.+x app 2>/dev/null; "
             "%s/ops/+x/ledger_peers.+x widget 2>/dev/null) | wc -l",
             project_root, project_root, project_root);
    int peer_count = 0;
    FILE *p = popen(count_cmd, "r");
    if (p) { char buf[64]; if (fgets(buf, sizeof(buf), p)) peer_count = atoi(buf); pclose(p); }

    if (key == ARROW_UP && cursor_pos > 0) cursor_pos--;
    if (key == ARROW_DOWN && cursor_pos < peer_count - 1) cursor_pos++;

    if ((key == 10 || key == 13) && peer_count > 0 && cursor_pos < peer_count) {
        /* Get the selected peer's session_root */
        char peer_cmd[MAX_PATH + 64];
        snprintf(peer_cmd, sizeof(peer_cmd),
                 "(%s/ops/+x/ledger_peers.+x editor 2>/dev/null; "
                 "%s/ops/+x/ledger_peers.+x app 2>/dev/null; "
                 "%s/ops/+x/ledger_peers.+x widget 2>/dev/null) | head -%d | tail -1 | cut -d'|' -f1",
                 project_root, project_root, project_root, cursor_pos + 1);
        char sr_buf[MAX_PATH] = "";
        FILE *sp = popen(peer_cmd, "r");
        if (sp) {
            if (fgets(sr_buf, sizeof(sr_buf), sp)) {
                sr_buf[strcspn(sr_buf, "\n")] = '\0';
                if (sr_buf[0]) set_focus(sr_buf);
            }
            pclose(sp);
        }
        write_kv(fm_path, "mode", "main_menu");
        write_kv(fm_path, "status_line", sr_buf[0] ? "Focused!" : "Focus failed");
        return;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", cursor_pos);
    write_kv(fm_path, "cursor_pos", buf);
}

int main(int argc, char **argv) {
    int key = (argc > 1) ? atoi(argv[1]) : 0;

    resolve_root();

    if (key != 0) {
        char fm_path[PATH_BUF], mode[64];
        snprintf(fm_path, sizeof(fm_path), "%s/pieces/system/fm_state.txt", project_root);
        read_kv(fm_path, "mode", mode, sizeof(mode));

        if (strcmp(mode, "main_menu") == 0 || !mode[0]) {
            handle_main_menu(fm_path, key);
        } else if (strcmp(mode, "file_browser") == 0) {
            handle_file_browser(fm_path, key);
        } else if (strcmp(mode, "peer_list") == 0) {
            handle_peer_list(fm_path, key);
        }
    }

    char marker[PATH_BUF];
    snprintf(marker, sizeof(marker), "%s/pieces/display/fm_screen_changed.txt", project_root);
    FILE *mf = fopen(marker, "a");
    if (mf) { fprintf(mf, "X\n"); fclose(mf); }

    return 0;
}
