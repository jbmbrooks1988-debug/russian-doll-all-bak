/* editor_widget_cmds - process widget/file-menu command inbox.
 *
 * Inbox:  pieces/system/widget_cmds/inbox.txt  (one cmd per line)
 * Status: pieces/system/widget_cmds/status.txt
 * Bridge: pieces/system/widget_bridge.txt      (paths for harness/widgets)
 *
 * Commands:
 *   NEW
 *   SAVE
 *   SAVE_AS:<path>
 *   LOAD:<path>
 *   PING
 *
 * Usage: editor_widget_cmds.+x [max_cmds]
 *   default max_cmds=32; processes up to that many lines then rewrites
 *   remaining inbox.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_LINE 4096
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_BUF 65536

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void ensure_cmd_dirs(void) {
    char d[PATH_BUF];
    snprintf(d, sizeof(d), "%s/pieces/system/widget_cmds", project_root);
    mkdir(d, 0755);
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

static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[48][MAX_LINE];
    int n = 0;
    if (f) {
        while (n < 48 && fgets(lines[n], MAX_LINE, f)) n++;
        fclose(f);
    }
    size_t klen = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strncmp(lines[i], key, klen) == 0 && lines[i][klen] == '=') {
            fprintf(f, "%s=%s\n", key, value);
            found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}

static void set_status(const char *cmd, const char *result, const char *msg) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/widget_cmds/status.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "last_cmd=%s\n", cmd ? cmd : "");
    fprintf(f, "result=%s\n", result ? result : "");
    fprintf(f, "message=%s\n", msg ? msg : "");
    fprintf(f, "at=%ld\n", (long)time(NULL));
    fclose(f);
}

static void publish_bridge(void) {
    ensure_cmd_dirs();
    char path[PATH_BUF], bufp[PATH_BUF], st[PATH_BUF], inb[PATH_BUF], statp[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/widget_bridge.txt", project_root);
    snprintf(bufp, sizeof(bufp), "%s/pieces/system/editor_buffer.txt", project_root);
    snprintf(st, sizeof(st), "%s/pieces/system/editor_state.txt", project_root);
    snprintf(inb, sizeof(inb), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    snprintf(statp, sizeof(statp), "%s/pieces/system/widget_cmds/status.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "session_root=%s\n", project_root);
    fprintf(f, "buffer_path=%s\n", bufp);
    fprintf(f, "state_path=%s\n", st);
    fprintf(f, "inbox_path=%s\n", inb);
    fprintf(f, "status_path=%s\n", statp);
    fprintf(f, "project_id=agy-editor\n");
    fprintf(f, "capabilities=file_document,text_buffer\n");
    fclose(f);
}

static size_t read_buffer(char *out, size_t out_sz) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_buffer.txt", project_root);
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, out_sz - 1, f);
    out[n] = '\0';
    fclose(f);
    return n;
}

static void write_buffer(const char *buf, size_t n) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/system/editor_buffer.txt", project_root);
    FILE *f = fopen(path, "w");
    if (!f) return;
    if (n) fwrite(buf, 1, n, f);
    fclose(f);
}

static void bump(void) {
    char p[PATH_BUF];
    snprintf(p, sizeof(p), "%s/pieces/display/editor_screen_changed.txt", project_root);
    FILE *f = fopen(p, "a");
    if (f) { fputc('.', f); fclose(f); }
}

static int do_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        set_status("LOAD", "error", "cannot open file");
        return -1;
    }
    char buf[MAX_BUF];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    write_buffer(buf, n);
    char st[PATH_BUF];
    snprintf(st, sizeof(st), "%s/pieces/system/editor_state.txt", project_root);
    write_kv(st, "file_path", path);
    write_kv(st, "cursor_pos", "-1");
    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Loaded %zu bytes from %s", n, path);
    write_kv(st, "last_message", msg);
    set_status("LOAD", "ok", msg);
    bump();
    return 0;
}

static int do_save_to(const char *path) {
    char buf[MAX_BUF];
    size_t n = read_buffer(buf, sizeof(buf));
    FILE *f = fopen(path, "w");
    if (!f) {
        set_status("SAVE", "error", "cannot write file");
        return -1;
    }
    if (n) fwrite(buf, 1, n, f);
    fclose(f);
    char st[PATH_BUF];
    snprintf(st, sizeof(st), "%s/pieces/system/editor_state.txt", project_root);
    write_kv(st, "file_path", path);
    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "Saved %zu bytes to %s", n, path);
    write_kv(st, "last_message", msg);
    set_status("SAVE", "ok", msg);
    bump();
    return 0;
}

static int do_new(void) {
    write_buffer("", 0);
    char st[PATH_BUF];
    snprintf(st, sizeof(st), "%s/pieces/system/editor_state.txt", project_root);
    write_kv(st, "file_path", "docs/untitled.txt");
    write_kv(st, "cursor_pos", "0");
    write_kv(st, "last_message", "NEW FILE via widget cmd");
    set_status("NEW", "ok", "buffer cleared");
    bump();
    return 0;
}

static int process_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    line[strcspn(line, "\r\n")] = '\0';
    if (!line[0] || line[0] == '#') return 0;

    if (strcmp(line, "PING") == 0) {
        set_status("PING", "ok", "pong");
        publish_bridge();
        return 0;
    }
    if (strcmp(line, "NEW") == 0) return do_new();
    if (strcmp(line, "SAVE") == 0) {
        char st[PATH_BUF], fp[MAX_PATH];
        snprintf(st, sizeof(st), "%s/pieces/system/editor_state.txt", project_root);
        read_kv(st, "file_path", fp, sizeof(fp));
        if (!fp[0]) {
            set_status("SAVE", "error", "no file_path — use SAVE_AS");
            return -1;
        }
        /* relative paths under project_root */
        char full[PATH_BUF];
        if (fp[0] == '/') snprintf(full, sizeof(full), "%s", fp);
        else snprintf(full, sizeof(full), "%s/%s", project_root, fp);
        return do_save_to(full);
    }
    if (strncmp(line, "SAVE_AS:", 8) == 0) {
        const char *p = line + 8;
        if (!p[0]) { set_status("SAVE_AS", "error", "empty path"); return -1; }
        return do_save_to(p);
    }
    if (strncmp(line, "LOAD:", 5) == 0) {
        const char *p = line + 5;
        if (!p[0]) { set_status("LOAD", "error", "empty path"); return -1; }
        return do_load(p);
    }
    set_status(line, "error", "unknown command");
    return -1;
}

int main(int argc, char **argv) {
    resolve_root();
    ensure_cmd_dirs();
    publish_bridge();

    int max_cmds = 32;
    if (argc >= 2) max_cmds = atoi(argv[1]);
    if (max_cmds < 1) max_cmds = 1;

    char inbox[PATH_BUF];
    snprintf(inbox, sizeof(inbox), "%s/pieces/system/widget_cmds/inbox.txt", project_root);
    FILE *f = fopen(inbox, "r");
    if (!f) {
        /* empty ok */
        set_status("IDLE", "ok", "no inbox");
        return 0;
    }

    char lines[64][MAX_LINE];
    int n = 0;
    while (n < 64 && fgets(lines[n], MAX_LINE, f)) n++;
    fclose(f);

    int processed = 0;
    int i = 0;
    for (; i < n && processed < max_cmds; i++) {
        if (lines[i][0] == '\0' || lines[i][0] == '\n') continue;
        process_line(lines[i]);
        processed++;
    }

    /* rewrite remaining */
    f = fopen(inbox, "w");
    if (f) {
        for (; i < n; i++) fputs(lines[i], f);
        fclose(f);
    }

    /* Do not clobber last LOAD/SAVE ACK with IDLE — harness and widgets
     * read status.txt after enqueue+drain; idle ticks must leave it. */
    (void)processed;
    return 0;
}
