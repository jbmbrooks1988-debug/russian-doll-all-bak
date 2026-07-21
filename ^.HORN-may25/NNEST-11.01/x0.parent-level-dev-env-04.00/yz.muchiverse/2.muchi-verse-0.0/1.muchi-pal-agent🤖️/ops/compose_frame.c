/* compose_frame - mutaclsym's compose_frame is a camera slicing a 2D map;
 * this project's equivalent tails the conversation log plus a status
 * line into pieces/apps/player_app/view.txt. Same role in the
 * architecture (the op responsible for turning state into a screen),
 * different content entirely - chat is a scrollback log, not a map.
 *
 * REAL <cli_io> UPGRADE (2026-07-20, CHAT-INTEGRATION-ARCHITECTURE.txt
 * sec. 1): this op used to also hand-draw the live input line with its
 * own manual "> text_" cursor, reading state.txt's own input_buffer -
 * that's now chtpm_parser_pal.c's own job (chat.chtpm's real
 * <cli_io>), so that whole block is gone, along with the input_buffer
 * read. ALSO: this op used to write pieces/display/current_frame.txt
 * DIRECTLY - a real violation of pal-standards.txt sec. 20's own ONE
 * VISIBLE FRAME WRITER RULE now that chtpm_parser_pal.c's own internal
 * compose_frame() is the sole legitimate writer of that file (a second
 * writer races it - confirmed, live, as real flicker/bugs in three
 * other projects this same family fixed the same way this session).
 * Fixed the same way every other project in this family already is:
 * write ONLY view.txt (which chat.chtpm's own ${game_map} reads), then
 * ping pieces/display/frame_changed.txt - the real render-trigger
 * marker.
 *
 * Self-contained: own root resolution, own constants, no shared headers.
 * Usage: compose_frame.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_FIELD 256
#define TAIL_TURNS 12

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_state_field(const char *state_path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(state_path, "r");
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

static char *pipe_unescape(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            i++;
            if (s[i] == 'n') *p++ = '\n';
            else *p++ = s[i];
        } else *p++ = s[i];
    }
    *p = '\0';
    return out;
}

typedef struct {
    char role[32];
    char kind[32];
    char tool_name[MAX_FIELD];
    char *content;
} Turn;

static void append_frame_history(const char *view_path) {
    char history_path[PATH_BUF];
    snprintf(history_path, sizeof(history_path), "%s/debug/frame_history.txt", project_root);
    FILE *hf = fopen(history_path, "a");
    if (!hf) return;

    time_t now = time(NULL);
    fprintf(hf, "\n=== FRAME @ %ld ===\n", now);

    FILE *vf = fopen(view_path, "r");
    if (vf) {
        char buf[MAX_LINE];
        while (fgets(buf, sizeof(buf), vf)) {
            fputs(buf, hf);
        }
        fclose(vf);
    }
    fprintf(hf, "---\n");
    fclose(hf);
}

int main(void) {
    resolve_root();

    char state_path[PATH_BUF], log_path[PATH_BUF], view_path[PATH_BUF];
    snprintf(state_path, sizeof(state_path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);

    char ai_state[MAX_FIELD], sys_msg[MAX_LINE], model_id[MAX_FIELD], api_url[MAX_FIELD];
    char pending_tool_name[MAX_FIELD], pending_tool_args[MAX_LINE];
    read_state_field(state_path, "ai_state", ai_state, sizeof(ai_state));
    read_state_field(state_path, "sys_msg", sys_msg, sizeof(sys_msg));
    read_state_field(state_path, "current_model_id", model_id, sizeof(model_id));
    read_state_field(state_path, "current_api_url", api_url, sizeof(api_url));
    read_state_field(state_path, "pending_tool_name", pending_tool_name, sizeof(pending_tool_name));
    read_state_field(state_path, "pending_tool_args", pending_tool_args, sizeof(pending_tool_args));

    /* Read every turn, keep only the last TAIL_TURNS in a ring buffer -
     * same "read everything, display only the tail" shape mutaclsym's
     * message_log.txt convention already established. */
    Turn turns[TAIL_TURNS];
    memset(turns, 0, sizeof(turns));
    int count = 0, total = 0;
    FILE *lf = fopen(log_path, "r");
    if (lf) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), lf)) {
            line[strcspn(line, "\n")] = '\0';
            char *p1 = strchr(line, '|'); if (!p1) continue;
            char *p2 = strchr(p1 + 1, '|'); if (!p2) continue;
            char *p3 = strchr(p2 + 1, '|'); if (!p3) continue;
            *p1 = '\0'; *p2 = '\0'; *p3 = '\0';
            int slot = total % TAIL_TURNS;
            /* Fields are always short in practice (role/kind/tool_name are
             * fixed vocabularies this project writes itself), but they're
             * substrings of a MAX_LINE-sized buffer as far as gcc's static
             * view goes - truncation is safe (fixed-size display fields),
             * just not provable at compile time. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(turns[slot].role, sizeof(turns[slot].role), "%s", line);
            snprintf(turns[slot].kind, sizeof(turns[slot].kind), "%s", p1 + 1);
            snprintf(turns[slot].tool_name, sizeof(turns[slot].tool_name), "%s", p2 + 1);
#pragma GCC diagnostic pop
            if (turns[slot].content) free(turns[slot].content);
            turns[slot].content = pipe_unescape(p3 + 1);
            total++;
        }
        fclose(lf);
    }
    count = total < TAIL_TURNS ? total : TAIL_TURNS;
    int start = total < TAIL_TURNS ? 0 : total % TAIL_TURNS;

    FILE *out = fopen(view_path, "w");
    if (!out) return 1;

    fprintf(out, "================================================================================\n");
    fprintf(out, " MUCHI-PAL-CHAT   [%s]   model: %-24s api: %s\n", ai_state, model_id, api_url);
    fprintf(out, "================================================================================\n\n");

    for (int i = 0; i < count; i++) {
        Turn *t = &turns[(start + i) % TAIL_TURNS];
        if (strcmp(t->role, "user") == 0) {
            fprintf(out, "You: %s\n\n", t->content);
        } else if (strcmp(t->role, "assistant") == 0 && strcmp(t->kind, "tool_call") == 0) {
            fprintf(out, "Aida: [calling %s %s]\n\n", t->tool_name, t->content);
        } else if (strcmp(t->role, "assistant") == 0) {
            fprintf(out, "Aida: %s\n\n", t->content);
        } else if (strcmp(t->role, "tool") == 0) {
            fprintf(out, "[%s result]: %s\n\n", t->tool_name, t->content);
        }
    }

    fprintf(out, "--------------------------------------------------------------------------------\n");
    if (strcmp(ai_state, "PENDING_PERM") == 0) {
        fprintf(out, "EXECUTE %s %s ? (type y/n below and press Enter)\n", pending_tool_name, pending_tool_args);
    } else if (strcmp(ai_state, "THINKING") == 0) {
        fprintf(out, "Thinking...\n");
    }
    fprintf(out, "--------------------------------------------------------------------------------\n");
    fprintf(out, "[SYS]: %s\n", sys_msg);
    fprintf(out, "(type /model <id> to switch models)\n");

    fclose(out);
    append_frame_history(view_path);

    /* pal-standards.txt sec. 20's own render-trigger marker - project-specific
     * chat_screen_changed.txt instead of generic frame_changed.txt so we don't
     * trigger on every keystroke. Matches pal-chat-irc pattern. */
    char marker_path[PATH_BUF];
    snprintf(marker_path, sizeof(marker_path), "%s/pieces/display/chat_screen_changed.txt", project_root);
    FILE *mf = fopen(marker_path, "a");
    if (mf) { fputc('.', mf); fclose(mf); }

    return 0;
}
