/* path_nav_manager - persistent live-typing path completion for chat.chtpm.
 *
 * User request (2026-07-20 handoff chain, see #.haiku/sonnet/): "*" typed
 * anywhere in the chat message is meant to collide with ordinary chat text
 * containing a literal "*" - same tradeoff as gem-dev's own completion
 * trigger, confirmed acceptable by direct instruction ("it is what it
 * is"), not a bug to guard against.
 *
 * Pattern ported from gem-dev's manager/gem-dev_manager.c
 * (update_completions/handle_choose_path) and its ops/src/complete_path.c,
 * but this project has no persistent AI-query-loop manager of its own
 * (send_message.c/check_response.c are one-shot ops instead - see that
 * file's own header comment), so this is a NEW, narrowly-scoped
 * persistent process, launched as a second <module> line alongside the
 * existing `system/prisc+x pal/main_loop_chtpm.pal` one. Confirmed safe
 * to coexist: main_loop_chtpm.pal's own read_history of interact_relay.txt
 * only ever branches on key==13 (Enter); every digit key this process
 * injects for file selection (0-9) falls through to that script's own
 * no_key/sleep branch untouched.
 *
 * Tree building is the real difference from gem-dev: gem-dev's picker is
 * flat (one directory level per query, re-invoked as the user retypes a
 * deeper path). This one recursively pre-builds nested <button> markup
 * (matched dir open "[-]", its own children pre-listed but collapsed
 * "[+]") because chtpm_parser_pal.c's native fold support (search
 * "[+]"/"[-]" in a label, see its own header comment sec. on
 * fold_&lt;id&gt;) only toggles VISIBILITY of already-parsed children - it
 * does not re-invoke anything on expand, so any level the user might
 * expand has to already be present in the generated markup up front.
 * Depth is capped (MAX_DEPTH) to bound both output size and directory
 * walk cost. Style verified directly against
 * 1.TPMOS_c_+rmmp.0102.0028/projects/+-demo's own hand-written example of
 * this exact nested-button/fold shape.
 *
 * Selection dispatch: only files are selectable (onClick="KEY:0".."KEY:9",
 * capped at 10 live picks across the whole rendered tree - the same
 * digit-injection budget gem-dev's own 5-slot picker used, just doubled
 * since chtpm_parser_pal.c's inject_raw_key() only single-key-injects for
 * KEY:0..KEY:9, see its own send_command()). Directories are never
 * KEY-bound - expand/collapse is entirely the native fold click, no
 * dispatch code needed on this end at all. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#define MAX_PATH_LEN 4096
#define MAX_MARKUP 65536
#define MAX_PICKS 10
#define MAX_DEPTH 6
#define MAX_ENTRIES_PER_DIR 40

static volatile sig_atomic_t g_shutdown = 0;
static void handle_sig(int s) { (void)s; g_shutdown = 1; }

static bool g_nav_mode = false;
static char g_last_typed[2048] = "";
static char g_nav_menu[MAX_MARKUP] = "";
static char g_pick_paths[MAX_PICKS][MAX_PATH_LEN];
static int g_pick_count = 0;
static long g_relay_last_pos = -1;

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static void pulse_render(void) {
    FILE *f = fopen("pieces/display/frame_changed.txt", "a");
    if (f) { fputs("N\n", f); fclose(f); }
}

static void write_gui_state(void) {
    FILE *f = fopen("projects/muchi-pal-agent/manager/gui_state.txt", "w");
    if (!f) return;
    fprintf(f, "nav_menu=%s\n", g_nav_menu);
    fclose(f);
}

/* Escapes a path for safe use inside a double-quoted chtpm label/attribute:
 * chtpm attribute parsing splits on '"', so a literal '"' in a filename
 * would break the generated markup - strip anything that isn't a plain
 * path character rather than risk malformed tags from untrusted dirents. */
static void sanitize_for_markup(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        char c = in[i];
        if (c == '"' || c == '<' || c == '>' || c == '&') continue;
        out[j++] = c;
    }
    out[j] = '\0';
}

static void sanitize_id(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        out[j++] = isalnum((unsigned char)in[i]) ? in[i] : '_';
    }
    out[j] = '\0';
}

/* Appends one directory's contents as nested <button> markup into buf.
 * dir_path is relative to cwd (the session root); rel_path is what's
 * shown to the user (same value here - no separate sandbox root in this
 * project). depth 0 is the top matched level (rendered open, "[-]");
 * deeper levels default collapsed ("[+]") per +-demo's own convention. */
static void append_dir_markup(char *buf, size_t buf_sz, const char *dir_path,
                               const char *filter_prefix, int depth) {
    DIR *d = opendir(dir_path[0] ? dir_path : ".");
    if (!d) return;

    struct dirent *entries[MAX_ENTRIES_PER_DIR];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < MAX_ENTRIES_PER_DIR) {
        if (e->d_name[0] == '.') continue;
        if (filter_prefix[0] && strncmp(e->d_name, filter_prefix, strlen(filter_prefix)) != 0) continue;
        /* readdir's own buffer is reused per-call, so copy the entry out
         * now rather than storing the dirent pointer past the next call. */
        struct dirent *copy = malloc(sizeof(struct dirent));
        if (copy) { memcpy(copy, e, sizeof(struct dirent)); entries[n++] = copy; }
    }
    closedir(d);

    /* Simple insertion sort by name - n is capped small (MAX_ENTRIES_PER_DIR). */
    for (int i = 1; i < n; i++) {
        struct dirent *key = entries[i];
        int j = i - 1;
        while (j >= 0 && strcmp(entries[j]->d_name, key->d_name) > 0) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }

    for (int i = 0; i < n; i++) {
        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s%s%s", dir_path, dir_path[0] ? "/" : "", entries[i]->d_name);
        struct stat st;
        bool is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));

        char safe_name[300], safe_id[300];
        sanitize_for_markup(entries[i]->d_name, safe_name, sizeof(safe_name));
        sanitize_id(full, safe_id, sizeof(safe_id));

        char line[1024];
        if (is_dir) {
            const char *marker = (depth == 0) ? "[-]" : "[+]";
            snprintf(line, sizeof(line), "<button label=\"%s/ %s\" id=\"nav_%s\">\n",
                     safe_name, marker, safe_id);
            strncat(buf, line, buf_sz - strlen(buf) - 1);
            strncat(buf, "<br/>\n", buf_sz - strlen(buf) - 1);

            if (depth + 1 < MAX_DEPTH) {
                append_dir_markup(buf, buf_sz, full, "", depth + 1);
            }
            strncat(buf, "</button><br/>\n", buf_sz - strlen(buf) - 1);
        } else if (g_pick_count < MAX_PICKS) {
            int idx = g_pick_count;
            snprintf(g_pick_paths[idx], MAX_PATH_LEN, "%s", full);
            g_pick_count++;
            snprintf(line, sizeof(line), "<button label=\"%s\" onClick=\"KEY:%d\" id=\"nav_%s\" /><br/>\n",
                     safe_name, idx, safe_id);
            strncat(buf, line, buf_sz - strlen(buf) - 1);
        }
        if (strlen(buf) + 512 >= buf_sz) break;
    }
    for (int i = 0; i < n; i++) free(entries[i]);
}

/* Splits "dir/prefix" into the directory to scan and the name-prefix to
 * filter on, same convention as gem-dev's own complete_path.c: everything
 * after the last '/' is the filter, everything before it is the dir (no
 * slash at all -> scan "." with the whole string as filter). */
static void update_nav_menu(const char *path_query) {
    g_pick_count = 0;
    g_nav_menu[0] = '\0';

    char dir_path[MAX_PATH_LEN] = "";
    const char *filter = path_query;
    const char *last_slash = strrchr(path_query, '/');
    if (last_slash) {
        size_t dlen = (size_t)(last_slash - path_query);
        if (dlen == 0) snprintf(dir_path, sizeof(dir_path), "/");
        else { size_t cp = dlen < sizeof(dir_path) - 1 ? dlen : sizeof(dir_path) - 1; memcpy(dir_path, path_query, cp); dir_path[cp] = '\0'; }
        filter = last_slash + 1;
    }

    append_dir_markup(g_nav_menu, sizeof(g_nav_menu), dir_path, filter, 0);
    write_gui_state();
    pulse_render();
}

static void exit_nav_mode(void) {
    g_nav_mode = false;
    g_nav_menu[0] = '\0';
    g_pick_count = 0;
    write_gui_state();
    pulse_render();
}

/* Splices the chosen file's path into the live input line, replacing
 * everything from the trigger '*' onward - mirrors gem-dev's own
 * handle_choose_path() truncate-at-star-then-append approach. */
static void choose_path(int idx) {
    if (idx < 0 || idx >= g_pick_count) { exit_nav_mode(); return; }

    char before[1024] = "";
    char *star = strrchr(g_last_typed, '*');
    if (star) {
        size_t blen = (size_t)(star - g_last_typed);
        if (blen >= sizeof(before)) blen = sizeof(before) - 1;
        memcpy(before, g_last_typed, blen);
        before[blen] = '\0';
    }

    char new_line[2048];
    snprintf(new_line, sizeof(new_line), "%s%s", before, g_pick_paths[idx]);

    FILE *f = fopen("pieces/apps/player_app/cli_buffers.txt", "a");
    if (f) { fprintf(f, "m%s\n", new_line); fclose(f); }

    snprintf(g_last_typed, sizeof(g_last_typed), "%s", new_line);
    exit_nav_mode();
}

/* Reads the last "m..."-prefixed line from cli_buffers.txt - the live
 * typed content of the message_input cli_io (prefix is the first char of
 * its id, "message_input" -> 'm', per chtpm_parser_pal.c's own generic
 * per-keystroke fallback append). Small file, re-scanned whole each tick
 * rather than tracked incrementally - simpler and cheap at this size. */
static bool read_last_typed(char *out, size_t out_sz) {
    FILE *f = fopen("pieces/apps/player_app/cli_buffers.txt", "r");
    if (!f) return false;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'm') {
            char *content = line + 1;
            content[strcspn(content, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", content);
            found = true;
        }
    }
    fclose(f);
    return found;
}

/* Polls interact_relay.txt (appended to by chtpm_parser_pal.c's own
 * KEY:n onClick dispatch, see send_command()'s "KEY:" branch) for newly
 * appended digit lines, from this process's own tracked offset - same
 * incremental-tail idea as gem-dev's g_buf_last_pos, just against the
 * relay file instead of the input-buffer log. */
static void poll_relay_for_picks(void) {
    const char *path = "pieces/apps/player_app/interact_relay.txt";
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (g_relay_last_pos < 0) {
        fseek(f, 0, SEEK_END);
        g_relay_last_pos = ftell(f);
        fclose(f);
        return;
    }
    fseek(f, g_relay_last_pos, SEEK_SET);
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        if (g_nav_mode) {
            char *t = trim(line);
            if (*t && isdigit((unsigned char)t[0]) && t[1] == '\0') {
                choose_path(t[0] - '0');
            }
        }
    }
    g_relay_last_pos = ftell(f);
    fclose(f);
}

int main(void) {
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    while (!g_shutdown) {
        char typed[1024] = "";
        bool have_line = read_last_typed(typed, sizeof(typed));

        if (have_line && strcmp(typed, g_last_typed) != 0) {
            snprintf(g_last_typed, sizeof(g_last_typed), "%s", typed);
            char *star = strrchr(g_last_typed, '*');
            if (star) {
                g_nav_mode = true;
                update_nav_menu(star + 1);
            } else if (g_nav_mode) {
                exit_nav_mode();
            }
        }

        poll_relay_for_picks();

        usleep(g_nav_mode ? 40000 : 150000);
    }
    return 0;
}
