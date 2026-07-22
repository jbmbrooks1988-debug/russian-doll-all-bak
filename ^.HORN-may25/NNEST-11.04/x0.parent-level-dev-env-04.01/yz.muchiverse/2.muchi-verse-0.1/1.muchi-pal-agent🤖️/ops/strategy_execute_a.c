/* strategy_execute_a.c - Strategy A handler: pre-execute detected tool
 *
 * When gemma_strategy.c sets selected_strategy=A and detected_tool=X,
 * this op:
 * 1. Parses the user message to extract arguments for the tool
 * 2. Calls the appropriate tool op (list_dir, read_file, exec_cmd, etc.)
 * 3. Captures the output
 * 4. Appends result to context_log.txt as a "tool" result entry
 *
 * Usage: strategy_execute_a.+x (reads state.txt, gui_state.txt)
 * Side effects: appends to context_log.txt, modifies state.txt
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 4096
#define MAX_CMD 8192

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void read_gui_state(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/projects/muchi-pal-agent/manager/gui_state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void read_state_field(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}

static void write_state_field(const char *key, const char *value) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/pieces/world_01/session_01/chat/state.txt", project_root);
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int nlines = 0;
    if (f) {
        while (nlines < 64 && fgets(lines[nlines], MAX_LINE, f)) nlines++;
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

static char *pipe_escape(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s);
    char *out = malloc(len * 2 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (s[i] == '|') { *p++ = '\\'; *p++ = '|'; }
        else if (s[i] == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else *p++ = s[i];
    }
    *p = '\0';
    return out;
}

static void append_log_turn(const char *log_path, const char *role, const char *kind, const char *tool_name, const char *content) {
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    char *esc_content = pipe_escape(content);
    fprintf(f, "%s|%s|%s|%s\n", role, kind, tool_name ? tool_name : "", esc_content);
    free(esc_content);
    fclose(f);
}

static char *run_tool_exec(const char **argv) {
    pid_t pid = fork();
    if (pid == -1) return strdup("Error: fork failed");

    if (pid == 0) {
        execvp(argv[0], (char * const *)argv);
        perror("execvp");
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return strdup("Tool executed successfully");
    }
    return strdup("Tool execution failed");
}

static char *extract_argument_after(const char *msg, const char *keyword) {
    char *hit = strcasestr(msg, keyword);
    if (!hit) return strdup("");

    char *p = hit + strlen(keyword);
    while (*p && isspace(*p)) p++;

    if (!*p) return strdup("");

    size_t len = strlen(p);
    char *result = malloc(len + 1);
    strcpy(result, p);
    return result;
}

int main(void) {
    resolve_root();

    char detected_tool[256] = "";
    char strategy[32] = "";
    char user_message[MAX_LINE] = "";

    read_state_field("detected_tool", detected_tool, sizeof(detected_tool));
    read_state_field("selected_strategy", strategy, sizeof(strategy));
    read_gui_state("message_input", user_message, sizeof(user_message));

    if (strcmp(strategy, "A") != 0 || strcmp(detected_tool, "none") == 0 || !user_message[0]) {
        return 0;
    }

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/world_01/session_01/chat/context_log.txt", project_root);

    char strategy_msg[256];
    snprintf(strategy_msg, sizeof(strategy_msg), "[Strategy A] Detected tool: %s", detected_tool);

    /* OUTPUT PATH 1: Append to context_log (appears in chat history) */
    append_log_turn(log_path, "system", "strategy", detected_tool, strategy_msg);

    /* OUTPUT PATH 2: Append to strategy_messages.txt */
    char strat_path[PATH_BUF];
    snprintf(strat_path, sizeof(strat_path), "%s/pieces/world_01/session_01/chat/strategy_messages.txt", project_root);
    FILE *sf = fopen(strat_path, "a");
    if (sf) {
        fprintf(sf, "%s\n", strategy_msg);
        fclose(sf);
    }

    /* OUTPUT PATH 3: Write to debug file */
    char debug_path[PATH_BUF];
    snprintf(debug_path, sizeof(debug_path), "%s/pieces/world_01/session_01/chat/strategy_debug.txt", project_root);
    FILE *dbg = fopen(debug_path, "a");
    if (dbg) {
        fprintf(dbg, "[strategy_execute_a] %s at %ld\n", strategy_msg, time(NULL));
        fclose(dbg);
    }

    /* OUTPUT PATH 4: Write to all_debug.txt (catch-all) */
    char all_debug[PATH_BUF];
    snprintf(all_debug, sizeof(all_debug), "%s/pieces/world_01/session_01/chat/all_debug.txt", project_root);
    FILE *adb = fopen(all_debug, "a");
    if (adb) {
        fprintf(adb, "[strategy_execute_a] %s\n", strategy_msg);
        fclose(adb);
    }

    /* OUTPUT PATH 5: Update state.txt sys_msg (shown in status line) */
    write_state_field("sys_msg", strategy_msg);

    /* OUTPUT PATH 6: Also write to detected_tool field so it's visible in state */
    write_state_field("detected_tool_strategy", strategy_msg);

    char *result = NULL;

    if (strcmp(detected_tool, "list_dir") == 0) {
        char cmd[MAX_CMD];
        snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/list_dir.+x'", project_root, project_root);
        FILE *pipe = popen(cmd, "r");
        if (pipe) {
            char buf[4096] = "";
            size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
            buf[n] = '\0';
            pclose(pipe);
            result = strdup(buf);
        } else {
            result = strdup("Error listing files");
        }
    } else if (strcmp(detected_tool, "read_file") == 0) {
        char *path = extract_argument_after(user_message, "read");
        if (path && strlen(path) > 0) {
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "cat '%s' 2>&1 | head -50", path);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error reading file");
            }
        }
        free(path);
    } else if (strcmp(detected_tool, "exec_cmd") == 0) {
        char *cmd_str = extract_argument_after(user_message, "run");
        if (cmd_str && strlen(cmd_str) > 0) {
            FILE *pipe = popen(cmd_str, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error executing command");
            }
        }
        free(cmd_str);
    } else if (strcmp(detected_tool, "speak") == 0) {
        char *text = extract_argument_after(user_message, "speak");
        if (text && strlen(text) > 0) {
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "PRISC_PROJECT_ROOT='%s' '%s/ops/+x/tts_speak.+x' '%s' 2>&1", project_root, project_root, text);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[1024] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: TTS failed");
            }
        }
        free(text);
    } else if (strcmp(detected_tool, "search_in_files") == 0) {
        char *query = extract_argument_after(user_message, "search");
        if (query && strlen(query) > 0) {
            char cmd[MAX_CMD];
            snprintf(cmd, sizeof(cmd), "grep -r '%s' . 2>/dev/null | head -20", query);
            FILE *pipe = popen(cmd, "r");
            if (pipe) {
                char buf[4096] = "";
                size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
                buf[n] = '\0';
                pclose(pipe);
                result = strdup(buf);
            } else {
                result = strdup("Error: search failed");
            }
        }
        free(query);
    }

    if (result && strlen(result) > 0) {
        append_log_turn(log_path, "tool", "result", detected_tool, result);
        char msg[256];
        snprintf(msg, sizeof(msg), "[Tool: %s] %zu bytes", detected_tool, strlen(result));
        write_state_field("sys_msg", msg);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "[Tool: %s] No output or error", detected_tool);
        write_state_field("sys_msg", msg);
    }

    write_state_field("detected_tool", "none");
    write_state_field("selected_strategy", "");
    free(result);
    return 0;
}
