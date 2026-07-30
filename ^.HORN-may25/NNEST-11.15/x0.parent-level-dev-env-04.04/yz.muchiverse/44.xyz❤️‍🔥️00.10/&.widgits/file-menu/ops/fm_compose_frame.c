#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_LINE 4096
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

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

static int has_focus(char *session_root, size_t sr_sz,
                     char *state_path, size_t sp_sz,
                     char *kind, size_t k_sz,
                     char *disp, size_t d_sz) {
    char focus[PATH_BUF];
    snprintf(focus, sizeof(focus), "%s/pieces/system/focus.txt", project_root);
    if (access(focus, F_OK) != 0) return 0;
    read_kv(focus, "session_root", session_root, sr_sz);
    read_kv(focus, "state_path", state_path, sp_sz);
    read_kv(focus, "kind", kind, k_sz);
    read_kv(focus, "display_name", disp, d_sz);
    if (!session_root[0] || access(session_root, F_OK) != 0) return 0;
    return 1;
}

static void format_size(long bytes, char *out, size_t out_sz) {
    if (bytes < 1024)
        snprintf(out, out_sz, "%ldB", bytes);
    else
        snprintf(out, out_sz, "%.0fKB", bytes / 1024.0);
}

static void append_footer(FILE *f, const char *status) {
    fprintf(f, "\n");
    fprintf(f, "  %s\n", status && status[0] ? status : "");
}

int main(void) {
    resolve_root();
    char fm_path[PATH_BUF], mode[64], browse_mode[64], browse_dir[PATH_BUF];
    char path_buffer[MAX_LINE], search_text[MAX_LINE], status_line[MAX_LINE];
    char cursor_str[64];
    int cursor_pos = 0;
    snprintf(fm_path, sizeof(fm_path), "%s/pieces/system/fm_state.txt", project_root);
    read_kv(fm_path, "mode", mode, sizeof(mode));
    read_kv(fm_path, "browse_mode", browse_mode, sizeof(browse_mode));
    read_kv(fm_path, "browse_dir", browse_dir, sizeof(browse_dir));
    read_kv(fm_path, "path_buffer", path_buffer, sizeof(path_buffer));
    read_kv(fm_path, "search_text", search_text, sizeof(search_text));
    read_kv(fm_path, "cursor_pos", cursor_str, sizeof(cursor_str));
    read_kv(fm_path, "status_line", status_line, sizeof(status_line));
    if (cursor_str[0]) cursor_pos = atoi(cursor_str);

    char session_root[MAX_PATH] = "", state_path[MAX_PATH] = "";
    char kind[128] = "", disp[128] = "";
    int focused = has_focus(session_root, sizeof(session_root),
                            state_path, sizeof(state_path),
                            kind, sizeof(kind), disp, sizeof(disp));

    char out_path[PATH_BUF];
    snprintf(out_path, sizeof(out_path), "%s/pieces/apps/player_app/view.txt", project_root);
    FILE *f = fopen(out_path, "w");
    if (!f) return 1;

    if (strcmp(mode, "file_browser") == 0) {
        const char *label = "LOAD";
        if (strcmp(browse_mode, "save_as") == 0) label = "SAVE AS";
        else if (strcmp(browse_mode, "pair") == 0) label = "PAIR WITH PROJECT";
        if (strcmp(browse_mode, "pair") == 0)
            fprintf(f, "  MODE: PAIR FROM FILE\n");
        else
            fprintf(f, "  MODE: %s FILE\n", label);
        fprintf(f, "  DIR: %s\n", browse_dir[0] ? browse_dir : ".");
        fprintf(f, "\n");

        int idx = 0;
        if (idx == cursor_pos)
            fprintf(f, " SEARCH: [>] %d. [%s]\n", idx + 1, search_text);
        else
            fprintf(f, " SEARCH: [ ] %d. [%s]\n", idx + 1, search_text);
        idx++;
        if (idx == cursor_pos)
            fprintf(f, " FILE:   [>] %d. [%s]\n", idx + 1, path_buffer);
        else
            fprintf(f, " FILE:   [ ] %d. [%s]\n", idx + 1, path_buffer);

        if (focused && state_path[0]) {
            char active[MAX_LINE] = "";
            read_kv(state_path, "file_path", active, sizeof(active));
            if (!active[0]) read_kv(state_path, "document", active, sizeof(active));
            fprintf(f, "\n");
            fprintf(f, "  ACTIVE: %s\n", active[0] ? active : "(none)");
        }
        fprintf(f, "\n");

        fprintf(f, "  DIRECTORY CONTENTS:\n");
        idx++;
        if (idx == cursor_pos)
            fprintf(f, "  [>] %d. [<- BACK]\n", idx + 1);
        else
            fprintf(f, "  [ ] %d. [<- BACK]\n", idx + 1);
        idx++;

        char filter[MAX_LINE] = "";
        if (search_text[0]) snprintf(filter, sizeof(filter), "%s", search_text);

        if (browse_dir[0]) {
            char scan_cmd[MAX_PATH + 64];
            if (filter[0])
                snprintf(scan_cmd, sizeof(scan_cmd), "%s/ops/+x/fm_scan_dir.+x \"%s\" \"%s\"",
                         project_root, browse_dir, filter);
            else
                snprintf(scan_cmd, sizeof(scan_cmd), "%s/ops/+x/fm_scan_dir.+x \"%s\"",
                         project_root, browse_dir);
            FILE *scan = popen(scan_cmd, "r");
            if (scan) {
                char line[MAX_LINE];
                while (fgets(line, sizeof(line), scan)) {
                    line[strcspn(line, "\n")] = '\0';
                    char kind_entry[16] = "", name[512] = "";
                    long size = 0;
                    int is_dir = 0;
                    sscanf(line, "%15[^|]|%511[^|]|%ld|%d", kind_entry, name, &size, &is_dir);
                    if (idx == cursor_pos)
                        fprintf(f, "  [>] %d. [%s] %s\n", idx + 1,
                                is_dir ? "DIR" : "FIL", name);
                    else
                        fprintf(f, "  [ ] %d. [%s] %s\n", idx + 1,
                                is_dir ? "DIR" : "FIL", name);
                    if (!is_dir && size > 0) {
                        char sz[32];
                        format_size(size, sz, sizeof(sz));
                        long pos = ftell(f);
                        fseek(f, pos - 1, SEEK_SET);
                        fprintf(f, " (%s)\n", sz);
                    }
                    idx++;
                }
                pclose(scan);
            }
        }

        fprintf(f, "\n");
        const char *confirm = (strcmp(browse_mode, "pair") == 0) ? "PAIR" : label;
        if (idx == cursor_pos)
            fprintf(f, "  [>] %d. [%s]\n", idx + 1, confirm);
        else
            fprintf(f, "  [ ] %d. [%s]\n", idx + 1, confirm);
        idx++;
        if (idx == cursor_pos)
            fprintf(f, "  [>] %d. [CANCEL]\n", idx + 1);
        else
            fprintf(f, "  [ ] %d. [CANCEL]\n", idx + 1);

        append_footer(f, status_line);

    } else if (strcmp(mode, "peer_list") == 0) {
        fprintf(f, "\n");
        fprintf(f, "  FOCUS TARGETS:\n");
        fprintf(f, "\n");

        int idx = 0;
        char scan_cmd[MAX_PATH + 64];
        snprintf(scan_cmd, sizeof(scan_cmd), "%s/ops/+x/ledger_peers.+x editor 2>/dev/null; "
                 "%s/ops/+x/ledger_peers.+x app 2>/dev/null; "
                 "%s/ops/+x/ledger_peers.+x widget 2>/dev/null",
                 project_root, project_root, project_root);
        FILE *peers = popen(scan_cmd, "r");
        if (peers) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), peers)) {
                line[strcspn(line, "\n")] = '\0';
                char sr[MAX_PATH] = "", inbox[MAX_PATH] = "", dn[256] = "", pid[64] = "", pi[128] = "";
                sscanf(line, "%4095[^|]|%4095[^|]|%255[^|]|%127[^|]|%63s",
                       sr, inbox, dn, pi, pid);
                const char *show = dn[0] ? dn : (pi[0] ? pi : sr);
                if (idx == cursor_pos)
                    fprintf(f, "  [>] %d. %s (PID %s)\n", idx + 1, show, pid);
                else
                    fprintf(f, "  [ ] %d. %s (PID %s)\n", idx + 1, show, pid);
                idx++;
            }
            pclose(peers);
        }
        if (idx > 0) {
            fprintf(f, "\n");
            fprintf(f, "  Select a target to pair. ESC to cancel.\n");
        } else {
            fprintf(f, "  No active projects found.\n");
            fprintf(f, "\n");
            fprintf(f, "  ESC to go back.\n");
        }

    } else {
        fprintf(f, "\n");
        if (focused) {
            fprintf(f, "  ACTIVE: %s\n", disp[0] ? disp : "(project)");
            if (state_path[0]) {
                char active[MAX_LINE] = "";
                read_kv(state_path, "file_path", active, sizeof(active));
                if (!active[0]) read_kv(state_path, "document", active, sizeof(active));
                if (active[0])
                    fprintf(f, "  FILE: %s\n", active);
            }
            fprintf(f, "\n");
            int idx = 1;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [NEW FILE]\n", idx);
            else
                fprintf(f, "  [ ] %d. [NEW FILE]\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [SAVE FILE]\n", idx);
            else
                fprintf(f, "  [ ] %d. [SAVE FILE]\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [SAVE AS...]\n", idx);
            else
                fprintf(f, "  [ ] %d. [SAVE AS...]\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [LOAD FILE...]\n", idx);
            else
                fprintf(f, "  [ ] %d. [LOAD FILE...]\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [CHANGE FOCUS]\n", idx);
            else
                fprintf(f, "  [ ] %d. [CHANGE FOCUS]\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [PAIR FROM FILE...]\n", idx);
            else
                fprintf(f, "  [ ] %d. [PAIR FROM FILE...]\n", idx);
        } else {
            fprintf(f, "  >> NO PROJECT FOCUSED <<\n");
            fprintf(f, "\n");
            int idx = 1;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. (no focus)\n", idx);
            else
                fprintf(f, "  [ ] %d. (no focus)\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. (no focus)\n", idx);
            else
                fprintf(f, "  [ ] %d. (no focus)\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. (no focus)\n", idx);
            else
                fprintf(f, "  [ ] %d. (no focus)\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. (no focus)\n", idx);
            else
                fprintf(f, "  [ ] %d. (no focus)\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [CHANGE FOCUS / PAIR]\n", idx);
            else
                fprintf(f, "  [ ] %d. [CHANGE FOCUS / PAIR]\n", idx);
            idx++;
            if (idx == cursor_pos)
                fprintf(f, "  [>] %d. [PAIR FROM FILE...]\n", idx);
            else
                fprintf(f, "  [ ] %d. [PAIR FROM FILE...]\n", idx);
        }
        fprintf(f, "\n");
        append_footer(f, status_line[0] ? status_line : (focused ? "Ready." : "Select 5 or 6 to pair with a project."));
    }

    fclose(f);
    return 0;
}
