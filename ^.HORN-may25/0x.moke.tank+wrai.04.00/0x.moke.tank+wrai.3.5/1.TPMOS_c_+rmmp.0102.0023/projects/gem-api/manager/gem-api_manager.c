#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/file.h>

#define PROJECT_ID "gem-api"
#define MAX_PATH 4096
#define MAX_LINE 1024

// PID tracking for CPU Safety
static void log_pid(pid_t pid, const char* name) {
    FILE *f = fopen("pieces/os/proc_list.txt", "a");
    if (f) {
        int fd = fileno(f);
        flock(fd, LOCK_EX);
        fprintf(f, "%d %s\n", pid, name);
        flock(fd, LOCK_UN);
        fclose(f);
    }
}

static char* get_gemini_api_key() {
    char* key_env = getenv("GEMINI_API_KEY");
    if (key_env && strlen(key_env) > 0) return strdup(key_env);
    FILE* f = fopen("config/google-lilsol-api-key.txt", "r");
    if (f) {
        char buf[256];
        if (fgets(buf, sizeof(buf), f)) {
            char* p = buf;
            while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
            size_t len = strlen(p);
            while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r' || p[len-1] == '\n')) {
                p[len-1] = '\0';
                len--;
            }
            fclose(f);
            if (len > 0) return strdup(p);
        } else {
            fclose(f);
        }
    }
    return NULL;
}

// Persistent Global UI State
static char g_ai_state[64] = "IDLE";
static char g_api_url[256] = "https://generativelanguage.googleapis.com";
static char g_current_model[256] = "llama3:latest";
static char g_ctx_pct[16] = "0%";
static char g_fsm_state[64] = "IDLE";
static char g_resp_area[8192] = "║ Ready for input...                                                         ║";
static char g_sys_msg[256] = "Initialized.";
static int g_thinking_secs = 0;
static time_t g_thinking_start = 0;
static char g_ai_status_line[128] = "";

// API Menu State
typedef struct {
    char name[64];
    char url[256];
} APIEntry;
static APIEntry g_api_list[16];
static int g_api_count = 0;
static char g_menu_area[4096] = "";

static char project_root[MAX_PATH] = ".";
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;
static volatile sig_atomic_t g_shutdown = 0;
static pid_t g_ai_pid = -1;
static char *g_pending_input = NULL;

// Pending Tool State for 'y/n' Permissions
static char g_pending_tool_name[128] = "";
static char *g_pending_args_json = NULL;
static char *g_pending_function_call = NULL;

// Completion State
static bool g_completion_mode = false;
static char g_completion_area[4096] = "";
static char g_completion_debug[128] = "Idle";
static char g_last_buf_input[1024] = "";
static long g_buf_last_pos = 0;

static void handle_sig(int s) { (void)s; g_shutdown = 1; }

// Forward Declarations
char* get_gui_var(const char* id);
void write_gui_state(void);
void start_ai_query(const char* input);
void format_response(const char* src);
void trigger_render(void);

static char* trim_str(char *str) {
    char *end;
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static char* run_tool(const char* tool_name, char* const args[], bool sandbox) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        close(pipefd[0]); dup2(pipefd[1], STDOUT_FILENO); close(pipefd[1]);
        char *full_path = NULL;
        if (sandbox) {
            if (chdir("projects/gem-api/sandbox") != 0) _exit(1);
            if (strchr(tool_name, '/') || tool_name[0] == '.') { execvp(tool_name, args); }
            else { 
                if (asprintf(&full_path, "../ops/+x/%s", tool_name) != -1) { execvp(full_path, args); free(full_path); }
                execvp(tool_name, args); // Fallback to system path
            }
        } else {
            if (strchr(tool_name, '/') || tool_name[0] == '.') { execvp(tool_name, args); }
            else { 
                if (asprintf(&full_path, "projects/gem-api/ops/+x/%s", tool_name) != -1) { execvp(full_path, args); free(full_path); }
                execvp(tool_name, args); // Fallback to system path
            }
        }
        _exit(127);
    }
    if (pid > 0) log_pid(pid, "gem-api-tool");
    close(pipefd[1]);
    char* output = malloc(ctx_limit);
    size_t total = 0;
    while (1) {
        char buf[1024];
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) break;
        if (total + n < ctx_limit) { memcpy(output + total, buf, n); total += n; }
    }
    output[total] = '\0';
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    size_t len = strlen(output); while (len > 0 && (output[len-1] == '\n' || output[len-1] == '\r')) output[--len] = '\0';
    return output;
}

static void load_apis(void) {
    FILE *f = fopen("projects/gem-api/config/apis.txt", "r");
    if (!f) { snprintf(g_sys_msg, sizeof(g_sys_msg), "Error: Could not open config/apis.txt"); return; }
    g_api_count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && g_api_count < 16) {
        char *sep = strchr(line, '|');
        if (sep) {
            *sep = '\0';
            strncpy(g_api_list[g_api_count].name, trim_str(line), 63);
            strncpy(g_api_list[g_api_count].url, trim_str(sep + 1), 255);
            g_api_count++;
        }
    }
    fclose(f);
    char *msg = NULL; if (asprintf(&msg, "Loaded %d APIs.", g_api_count) != -1) { snprintf(g_sys_msg, sizeof(g_sys_msg), "%s", msg); free(msg); }
}

static void update_menu_markup(void) {
    char buf[4096] = "";
    for (int i = 0; i < g_api_count; i++) {
        char line[512];
        snprintf(line, sizeof(line), "        <button label=\"%s\" onClick=\"SET_API:%s\" /><br/>", g_api_list[i].name, g_api_list[i].url);
        strcat(buf, line);
    }
    snprintf(g_menu_area, sizeof(g_menu_area), "%s", buf);
}

static char* read_full_file(const char* path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

void format_response(const char* src) {
    char formatted[8192] = "";
    char line_buf[512];
    const char* p = src;
    while (*p && strlen(formatted) < 7000) {
        int len = 0;
        while (p[len] && p[len] != '\n' && len < 74) len++;
        char wrap[75];
        memcpy(wrap, p, len); wrap[len] = '\0';
        snprintf(line_buf, sizeof(line_buf), "║ %-74.74s ║\\n", wrap);
        strcat(formatted, line_buf);
        p += len;
        if (*p == '\n') p++;
    }
    if (strlen(formatted) > 2) formatted[strlen(formatted)-2] = '\0';
    snprintf(g_resp_area, sizeof(g_resp_area), "%s", formatted);
}

void execute_pending_tool(void) {
    if (strlen(g_pending_tool_name) == 0) return;
    char *ctx_file = "projects/gem-api/state/context.json";
    char *tmp_args = "projects/gem-api/state/args.tmp";

    snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "%s", g_pending_tool_name);
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Executing %s...", g_pending_tool_name);
    
    FILE *fargs = fopen(tmp_args, "w");
    if (fargs) { fputs(g_pending_args_json ? g_pending_args_json : "{}", fargs); fclose(fargs); }
    
    char *result = NULL;
    if (strcmp(g_pending_tool_name, "read_file") == 0) {
        char *p_p[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "path", NULL};
        char *path = run_tool(p_p[0], p_p, false);
        if (path && strlen(path) > 0) { char *ta[] = {"file_ops", "read", path, NULL}; result = run_tool("file_ops", ta, true); }
        if (path) free(path);
    } else if (strcmp(g_pending_tool_name, "write_file") == 0) {
        char *p_p[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "path", NULL};
        char *p_c[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "content", NULL};
        char *path = run_tool(p_p[0], p_p, false); char *cont = run_tool(p_c[0], p_c, false);
        if (path && strlen(path) > 0 && cont) { char *ta[] = {"file_ops", "write", path, cont, NULL}; result = run_tool("file_ops", ta, true); }
        if (path) free(path); if (cont) free(cont);
    } else if (strcmp(g_pending_tool_name, "list_dir") == 0) {
        char *p_p[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "path", NULL};
        char *path = run_tool(p_p[0], p_p, false); 
        char *ta[] = {"list_dir", (path && strlen(path) > 0) ? path : ".", NULL}; 
        result = run_tool("list_dir", ta, true); 
        if (path) free(path);
    } else if (strcmp(g_pending_tool_name, "exec_cmd") == 0) {
        char *p_c[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "cmd", NULL};
        char *cmd = run_tool(p_c[0], p_c, false); if (cmd && strlen(cmd) > 0) { char *ta[] = {"cmd_exec", cmd, NULL}; result = run_tool("cmd_exec", ta, true); }
        if (cmd) free(cmd);
    } else if (strcmp(g_pending_tool_name, "search_in_files") == 0) {
        char *p_q[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "query", NULL};
        char *query = run_tool(p_q[0], p_q, false); if (query && strlen(query) > 0) { char *ta[] = {"search_in_files", query, NULL}; result = run_tool("search_in_files", ta, true); }
        if (query) free(query);
    } else if (strcmp(g_pending_tool_name, "edit_file") == 0) {
        char *p_p[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "path", NULL};
        char *p_s[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "search", NULL};
        char *p_r[] = {"projects/gem-api/ops/+x/json_parser", tmp_args, "replace", NULL};
        char *path = run_tool(p_p[0], p_p, false); char *search = run_tool(p_s[0], p_s, false); char *replace = run_tool(p_r[0], p_r, false);
        if (path && strlen(path) > 0 && search && replace) { char *ta[] = {"edit_file", path, search, replace, NULL}; result = run_tool("edit_file", ta, true); }
        if (path) free(path); if (search) free(search); if (replace) free(replace);
    }

    if (result) {
        char *a_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "assistant", g_pending_function_call, NULL};
        run_tool(a_args[0], a_args, false);
        char *t_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "tool", result, NULL};
        run_tool(t_args[0], t_args, false);
        free(result);
    } else {
        char *a_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "assistant", g_pending_function_call, NULL};
        run_tool(a_args[0], a_args, false);
        char *t_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "tool", "Error: Tool execution failed or unknown tool.", NULL};
        run_tool(t_args[0], t_args, false);
    }

    if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
    if (g_pending_function_call) { free(g_pending_function_call); g_pending_function_call = NULL; }
    g_pending_tool_name[0] = '\0';
    start_ai_query(NULL);
}

void deny_pending_tool(void) {
    if (strlen(g_pending_tool_name) == 0) return;
    char *ctx_file = "projects/gem-api/state/context.json";
    snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Tool execution denied by user.");
    
    char *a_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "assistant", g_pending_function_call, NULL};
    run_tool(a_args[0], a_args, false);
    char *t_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "tool", "Error: Permission denied by user.", NULL};
    run_tool(t_args[0], t_args, false);
    
    if (g_pending_args_json) { free(g_pending_args_json); g_pending_args_json = NULL; }
    if (g_pending_function_call) { free(g_pending_function_call); g_pending_function_call = NULL; }
    g_pending_tool_name[0] = '\0';
    start_ai_query(NULL);
}

static void append_aligned_suggestion(char *out, size_t max_sz, const char *label, const char *path) {
    char btn_markup[2048];
    // Box interior is 74 chars wide. "║  " (3) + label + padding + "  ║" (3)
    // Actually layout has 78 or 80. Let's use 74 as safe interior.
    int label_len = strlen(label);
    int padding = 66 - label_len; // Adjust for parser button numbering [N]
    if (padding < 0) padding = 0;
    
    snprintf(btn_markup, sizeof(btn_markup), "║  <button label=\"%s\" onClick=\"CHOOSE:%s\" />", label, path);
    strncat(out, btn_markup, max_sz - strlen(out) - 1);
    
    if (padding > 0) {
        char pad_str[128];
        snprintf(pad_str, sizeof(pad_str), "<text label=\"%.*s\" />", padding, "                                                                                ");
        strncat(out, pad_str, max_sz - strlen(out) - 1);
    }
    strncat(out, "  ║<br/>", max_sz - strlen(out) - 1);
}

void update_completions(const char *last_line) {
    char *star = strrchr(last_line, '*');
    if (!star) { 
        if (g_completion_mode) {
            g_completion_mode = false; 
            g_completion_area[0] = '\0'; 
        }
        return; 
    }
    
    g_completion_mode = true;
    char *prefix = star + 1;
    char *p_args[] = {"projects/gem-api/ops/+x/complete_path", prefix, NULL};
    char *matches = run_tool(p_args[0], p_args, false);
    
    g_completion_area[0] = '\0';
    if (matches && strlen(matches) > 0) {
        char *copy = strdup(matches);
        char *token = strtok(copy, "  ");
        int count = 0;
        while (token && count < 5) {
            char btn[1024];
            // Format to ensure it fits in the ║ ... ║ box
            snprintf(btn, sizeof(btn), "║ <button label=\"%-74.74s\" onClick=\"CHOOSE:%s\" /> ║<br/>", token, token);
            strcat(g_completion_area, btn);
            token = strtok(NULL, "  ");
            count++;
        }
        free(copy);
        free(matches);
    } else {
        snprintf(g_completion_area, sizeof(g_completion_area), "║ (no matches)                                                             ║<br/>");
    }
}

void handle_choose_path(const char *choice) {
    char *buf_path = "pieces/apps/player_app/cli_buffers.txt";
    FILE *bf = fopen(buf_path, "r");
    char last_line[1024] = "";
    if (bf) {
        char line[1024];
        while (fgets(line, sizeof(line), bf)) {
            if (line[0] == 'i') {
                strncpy(last_line, line + 1, sizeof(last_line) - 1);
                size_t l = strlen(last_line); if (l > 0 && last_line[l-1] == '\n') last_line[l-1] = '\0';
            }
        }
        fclose(bf);
    }

    if (strlen(last_line) > 0) {
        char *star = strrchr(last_line, '*');
        if (star) {
            *star = '\0';
            char new_input[2048];
            snprintf(new_input, sizeof(new_input), "i%s%s", last_line, choice);
            
            FILE *bfw = fopen(buf_path, "a");
            if (bfw) { fprintf(bfw, "%s\n", new_input); fclose(bfw); }
            
            // Also update gui_state.txt so Enter works correctly
            char *path = "projects/gem-api/manager/gui_state.txt";
            FILE *f = fopen(path, "a");
            if (f) { fprintf(f, "input_text=%s\n", new_input + 1); fclose(f); }
        }
    }
    g_completion_mode = false;
    g_completion_area[0] = '\0';
}

void audit_log(const char* user, const char* assistant) {
    char *path = "projects/gem-api/pieces/world_01/map_01/iqabel/memories/history.txt";
    mkdir("projects/gem-api/pieces/world_01/map_01/iqabel/memories", 0755);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t now = time(NULL);
        char *ts = ctime(&now); ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s]\nUSER: %s\nAGENT: %s\n\n", ts, user, assistant);
        fclose(f);
    }
}

static void resolve_local_model(void) {
    char* tags_file = "projects/gem-api/state/ollama_tags.json";
    char* api_path = NULL; 
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;

    if (is_llamacpp) {
        if (asprintf(&api_path, "%s/v1/models", g_api_url) == -1) return;
    } else {
        if (asprintf(&api_path, "%s/api/tags", g_api_url) == -1) return;
    }

    char* curl_args[] = {"curl", "-s", "--max-time", "5", api_path, "-o", tags_file, NULL};
    run_tool("curl", curl_args, false);
    free(api_path);

    char* key = is_llamacpp ? "id" : "name";
    char* find_args[] = {"projects/gem-api/ops/+x/json_parser", tags_file, key, NULL};
    char* models = run_tool(find_args[0], find_args, false);
    if (models) {
        char* copy = strdup(models);
        char* token = strtok(copy, "  ");
        int found = 0;
        while (token) {
            if (strstr(token, "groq-tool-use")) { strncpy(g_current_model, token, 255); found = 1; break; }
            token = strtok(NULL, "  ");
        }
        if (!found) {
            char* second_copy = strdup(models);
            token = strtok(second_copy, "  ");
            while (token) {
                if (strstr(token, "groq")) { strncpy(g_current_model, token, 255); found = 1; break; }
                token = strtok(NULL, "  ");
            }
            free(second_copy);
        }
        if (!found) {
            char* third_copy = strdup(models);
            char* first = strtok(third_copy, "  ");
            if (first) strncpy(g_current_model, first, 255);
            free(third_copy);
        }
        free(copy);
        free(models);
    }
}

void start_ai_query(const char* input) {
    char *ctx_file = "projects/gem-api/state/context.json";
    char *tmp_prompt = "projects/gem-api/state/prompt.json";
    char *tmp_llm = "projects/gem-api/state/llm_response.json";
    char *tmp_content = "projects/gem-api/state/llm_content.json";
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
    bool is_gemini = strstr(g_api_url, "generativelanguage") != NULL;

    if (g_ai_pid > 0) return;
    unlink(tmp_llm); unlink(tmp_content);
    struct stat st;
    if (stat(ctx_file, &st) != 0 || st.st_size < 10) {
        char *persona = read_full_file("config/persona.txt");
        if (persona && strlen(persona) > 5) {
            char *s_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "system", persona, NULL};
            run_tool(s_args[0], s_args, false);
            free(persona);
        } else {
            if (persona) free(persona);
            char *default_persona = "You are Aida, a technical agent. Respond in JSON with 'response' or 'tool' keys.";
            char *s_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "system", default_persona, NULL};
            run_tool(s_args[0], s_args, false);
        }
    }
    if (input) {
        if (g_pending_input) free(g_pending_input);
        g_pending_input = strdup(input);
        char *u_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "user", (char*)input, NULL};
        run_tool(u_args[0], u_args, false);
    }
    snprintf(g_ai_state, sizeof(g_ai_state), "THINKING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "THINKING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Querying AI...");
    g_thinking_start = time(NULL);
    g_thinking_secs = 0;
    char *r_args[] = {"projects/gem-api/ops/+x/json_state", "read", ctx_file, NULL};
    char *context = run_tool(r_args[0], r_args, false);
    
    char *p_args[] = {"projects/gem-api/ops/+x/gemini_payload_builder", ctx_file, tmp_prompt, NULL};
    run_tool(p_args[0], p_args, false);

    if (context) free(context);
    char *body_arg = NULL; char *api_path = NULL;
    
    if (is_llamacpp) {
        asprintf(&api_path, "%s/v1/chat/completions", g_api_url);
    } else if (is_gemini) {
        asprintf(&api_path, "%s/v1beta/models/gemini-2.5-flash:generateContent", g_api_url);
    } else {
        asprintf(&api_path, "%s/api/chat", g_api_url);
    }

    if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
        g_ai_pid = fork();
        if (g_ai_pid == 0) {
            setpgid(0, 0);
            char *curl_args[20];
            int arg_count = 0;
            curl_args[arg_count++] = "curl";
            curl_args[arg_count++] = "-v";
            curl_args[arg_count++] = "-s";
            curl_args[arg_count++] = "--max-time";
            curl_args[arg_count++] = "600";
            curl_args[arg_count++] = "-H";
            curl_args[arg_count++] = "Content-Type: application/json";
            
            char *key = NULL;
            if (is_gemini) {
                key = get_gemini_api_key();
                if (key) {
                    char *header = NULL;
                    asprintf(&header, "x-goog-api-key: %s", key);
                    curl_args[arg_count++] = "-H";
                    curl_args[arg_count++] = header;
                    free(key);
                }
            }
            
            curl_args[arg_count++] = api_path;
            curl_args[arg_count++] = "-d";
            curl_args[arg_count++] = body_arg;
            curl_args[arg_count++] = "-o";
            curl_args[arg_count++] = tmp_llm;
            curl_args[arg_count++] = NULL;

            int fd = open("projects/gem-api/state/curl_debug.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
            execvp("curl", curl_args);
            _exit(127);
        }
        if (g_ai_pid > 0) log_pid(g_ai_pid, "gem-api-curl");
    }
    free(body_arg); free(api_path);
}

void check_ai_status(void) {
    if (g_ai_pid <= 0) return;
    int status;
    pid_t res = waitpid(g_ai_pid, &status, WNOHANG);
    if (res == 0) return;
    g_ai_pid = -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        snprintf(g_sys_msg, sizeof(g_sys_msg), "API Error: Curl failed (code %d)", WEXITSTATUS(status));
        snprintf(g_ai_state, sizeof(g_ai_state), "IDLE");
        snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
        if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
        return;
    }
    char *ctx_file = "projects/gem-api/state/context.json";
    char *tmp_llm = "projects/gem-api/state/llm_response.json";
    char *tmp_last_txt = "projects/gem-api/state/last_response.txt";
    char *tmp_args = "projects/gem-api/state/args.tmp";

    char *text_content = NULL;
    char *function_call = NULL;

    // Gemini Response Structure: candidates[0].content.parts[0].text OR candidates[0].content.parts[0].functionCall
    char *p_text[] = {"projects/gem-api/ops/+x/json_parser", tmp_llm, "candidates[0].content.parts[0].text", NULL};
    text_content = run_tool(p_text[0], p_text, false);
    
    // Fallback for different Gemini versions or if already partially parsed
    if (!text_content || strlen(text_content) == 0) {
        if (text_content) free(text_content);
        char *p_text_fallback[] = {"projects/gem-api/ops/+x/json_parser", tmp_llm, "text", NULL};
        text_content = run_tool(p_text_fallback[0], p_text_fallback, false);
    }

    char *p_func[] = {"projects/gem-api/ops/+x/json_parser", tmp_llm, "candidates[0].content.parts[0].functionCall", NULL};
    function_call = run_tool(p_func[0], p_func, false);
    
    if (!function_call || strlen(function_call) == 0) {
        if (function_call) free(function_call);
        char *p_func_fallback[] = {"projects/gem-api/ops/+x/json_parser", tmp_llm, "functionCall", NULL};
        function_call = run_tool(p_func_fallback[0], p_func_fallback, false);
    }

    // Check for API-level errors if no content or function call
    if ((!text_content || strlen(text_content) == 0) && (!function_call || strlen(function_call) == 0)) {
        char *p_err[] = {"projects/gem-api/ops/+x/json_parser", tmp_llm, "error.message", NULL};
        char *error_msg = run_tool(p_err[0], p_err, false);
        if (error_msg && strlen(error_msg) > 0) {
            if (text_content) free(text_content);
            text_content = error_msg;
        }
    }

    char *tool_name = NULL; char *args_json = NULL;
    if (function_call && strlen(function_call) > 2) {
        FILE *ffunc = fopen("projects/gem-api/state/function_call.tmp", "w");
        if (ffunc) { fputs(function_call, ffunc); fclose(ffunc); }
        char *p_tn[] = {"projects/gem-api/ops/+x/json_parser", "projects/gem-api/state/function_call.tmp", "name", NULL};
        tool_name = run_tool(p_tn[0], p_tn, false);
        char *p_ta[] = {"projects/gem-api/ops/+x/json_parser", "projects/gem-api/state/function_call.tmp", "args", NULL};
        args_json = run_tool(p_ta[0], p_ta, false);
        unlink("projects/gem-api/state/function_call.tmp");
    }

    if (tool_name && strlen(tool_name) > 0) {
        // Defer execution and request permission
        strncpy(g_pending_tool_name, tool_name, sizeof(g_pending_tool_name) - 1);
        g_pending_args_json = args_json;         // Transfer ownership
        g_pending_function_call = function_call; // Transfer ownership
        
        snprintf(g_ai_state, sizeof(g_ai_state), "PENDING_PERM");
        snprintf(g_fsm_state, sizeof(g_fsm_state), "WAIT_PERM");
        snprintf(g_sys_msg, sizeof(g_sys_msg), "Execute %s? (y/n)", tool_name);
        
        char format_buf[4096];
        snprintf(format_buf, sizeof(format_buf), "Tool Execution Request\nTool: %s\nArgs: %s\n\nType 'y' to allow or 'n' to deny.", tool_name, args_json ? args_json : "{}");
        format_response(format_buf);
        
        free(tool_name);
        free(text_content);
        return; 
    }

    if (!text_content || strlen(text_content) == 0) {
        if (text_content) free(text_content);
        text_content = strdup("(empty response)");
    }

    // Write to last_response.txt for audit/read
    FILE *fresp = fopen(tmp_last_txt, "w");
    if (fresp) { fputs(text_content, fresp); fclose(fresp); }

    audit_log(g_pending_input, text_content);
    format_response(text_content);

    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); 
    snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Response received.");

    char *r_args_app[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "assistant", text_content, NULL};
    run_tool(r_args_app[0], r_args_app, false);

    free(text_content); free(function_call);
    if (g_pending_input) { free(g_pending_input); g_pending_input = NULL; }
}

void resolve_paths(const char* hint) {
    char kvp_path[MAX_PATH]; snprintf(kvp_path, sizeof(kvp_path), "%s/pieces/locations/location_kvp", hint ? hint : ".");
    FILE *kvp = fopen(kvp_path, "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; char *k = trim_str(line); char *v = trim_str(eq + 1); if (strcmp(k, "project_root") == 0) snprintf(project_root, sizeof(project_root), "%s", v); }
        }
        fclose(kvp);
    } else if (hint) snprintf(project_root, sizeof(project_root), "%s", hint);
}

void trigger_render(void) { char *path = "pieces/display/frame_changed.txt"; FILE *f = fopen(path, "a"); if (f) { fprintf(f, "M\n"); fclose(f); } }

char* get_gui_var(const char* id) {
    char *path = "projects/gem-api/manager/gui_state.txt"; char *line = NULL; size_t len = 0; FILE *f = fopen(path, "r");
    if (!f) return NULL;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, id, strlen(id)) == 0 && line[strlen(id)] == '=') {
            char *val = strdup(line + strlen(id) + 1); if (val[strlen(val)-1] == '\n') val[strlen(val)-1] = '\0';
            fclose(f); free(line); return val;
        }
    }
    fclose(f); free(line); return NULL;
}

void write_gui_state(void) {
    char *path = "projects/gem-api/manager/gui_state.txt"; char *existing_input = NULL; FILE *fr = fopen(path, "r");
    if (fr) {
        char *line = NULL; size_t len = 0;
        while (getline(&line, &len, fr) != -1) { if (strncmp(line, "input_text=", 11) == 0) { existing_input = strdup(line + 11); if (existing_input[strlen(existing_input)-1] == '\n') existing_input[strlen(existing_input)-1] = '\0'; } }
        free(line); fclose(fr);
    }

    if (strcmp(g_ai_state, "THINKING") == 0) {
        snprintf(g_ai_status_line, sizeof(g_ai_status_line), "[AI STATE]: %s (%ds) | [API]: %s", g_ai_state, g_thinking_secs, g_api_url);
    } else {
        snprintf(g_ai_status_line, sizeof(g_ai_status_line), "[AI STATE]: %s | [API]: %s", g_ai_state, g_api_url);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/gem-api/manager/+x/gem-api_manager.+x\n");
        fprintf(f, "active_layout_id=gem-api.chtpm\n");
        fprintf(f, "ai_state=%s\n", g_ai_state);
        fprintf(f, "active_api=%s\n", g_api_url);
        fprintf(f, "ai_status_line=%s\n", g_ai_status_line);
        fprintf(f, "ctx_pct=%s\n", g_ctx_pct);
        fprintf(f, "iqabel_fsm=%s\n", g_fsm_state);
        fprintf(f, "gem-api_response_area=%s\n", g_resp_area);
        fprintf(f, "gem-api_api_menu=%s\n", g_menu_area);
        fprintf(f, "completion_area=%s\n", g_completion_area);
        fprintf(f, "completion_debug=%s\n", g_completion_debug);
        fprintf(f, "sys_msg=%s\n", g_sys_msg);
        fprintf(f, "thinking_secs=%d\n", g_thinking_secs);
        if (existing_input) fprintf(f, "input_text=%s\n", existing_input);
        fclose(f);
    }
    free(existing_input);
}

void process_input_trigger(void) {
    char *input = get_gui_var("input_text");
    if (input && strlen(input) > 0) {
        char *trimmed = trim_str(input);
        if (strcmp(g_ai_state, "PENDING_PERM") == 0) {
            char *path = "projects/gem-api/manager/gui_state.txt";
            FILE *f = fopen(path, "a"); if (f) { fprintf(f, "input_text=\n"); fclose(f); }
            
            if (strcasecmp(trimmed, "y") == 0 || strcasecmp(trimmed, "yes") == 0) {
                execute_pending_tool();
            } else if (strcasecmp(trimmed, "n") == 0 || strcasecmp(trimmed, "no") == 0) {
                deny_pending_tool();
            } else {
                snprintf(g_sys_msg, sizeof(g_sys_msg), "Invalid response. Type 'y' or 'n' for %s.", g_pending_tool_name);
            }
        } else {
            char *path = "projects/gem-api/manager/gui_state.txt";
            FILE *f = fopen(path, "a"); if (f) { fprintf(f, "input_text=\n"); fclose(f); }
            start_ai_query(input);
        }
    }
    if (input) free(input);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sig); signal(SIGTERM, handle_sig); setpgid(0, 0); log_pid(getpid(), "gem-api-manager");
    resolve_paths(argc > 1 ? argv[1] : NULL);
    if (chdir(project_root) != 0) perror("chdir project_root failed");
    mkdir("projects/gem-api/state", 0755); mkdir("projects/gem-api/sandbox", 0755); 
    load_apis(); update_menu_markup();
    resolve_local_model();
    // Initialize last_pos to ignore old history
    struct stat st;
    char *hist_path = "pieces/apps/player_app/history.txt"; 
    long last_pos = 0; 
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    
    char *buf_path = "pieces/apps/player_app/cli_buffers.txt";
    g_buf_last_pos = 0;
    if (stat(buf_path, &st) == 0) g_buf_last_pos = st.st_size;
    while (!g_shutdown) {
        char *ctx_file = "projects/gem-api/state/context.json";
        int pct = 0; char *r_args_pct[] = {"projects/gem-api/ops/+x/json_state", "read", ctx_file, NULL};
        char *full_ctx = run_tool(r_args_pct[0], r_args_pct, false);
        if (full_ctx) { pct = (strlen(full_ctx) / ctx_divisor); if (pct > 100) pct = 100; free(full_ctx); }
        snprintf(g_ctx_pct, sizeof(g_ctx_pct), "%d%%", pct);
        int state_changed = 0;

        // Polling cli_buffers.txt for '*' completion trigger and updates
        char *buf_path = "pieces/apps/player_app/cli_buffers.txt";
        if (stat(buf_path, &st) == 0 && st.st_size > g_buf_last_pos) {
            FILE *bf = fopen(buf_path, "r");
            if (bf) {
                fseek(bf, g_buf_last_pos, SEEK_SET);
                char last_line[1024] = "";
                char line[1024];
                while (fgets(line, sizeof(line), bf)) {
                    if (line[0] == 'i') {
                        strncpy(last_line, line + 1, sizeof(last_line) - 1);
                        size_t l = strlen(last_line); if (l > 0 && last_line[l-1] == '\n') last_line[l-1] = '\0';
                    }
                }
                g_buf_last_pos = ftell(bf);
                fclose(bf);

                if (strcmp(last_line, g_last_buf_input) != 0) {
                    strncpy(g_last_buf_input, last_line, sizeof(g_last_buf_input) - 1);

                    char *star = strchr(last_line, '*');

                    if (star) {
                        snprintf(g_completion_debug, sizeof(g_completion_debug), "Star Detected at pos %ld", (long)(star - last_line));
                        g_completion_mode = true;
                        char *prefix = last_line; // Use full input for path completion tool
                        char *p_args[] = {"projects/gem-api/ops/+x/complete_path", prefix, NULL};
                        char *matches = run_tool(p_args[0], p_args, false);
                        
                        g_completion_area[0] = '\0';
                        if (matches && strlen(matches) > 0) {
                            char *copy = strdup(matches);
                            char *token = strtok(copy, "  ");
                            int count = 0;
                            while (token && count < 5) {
                                char btn[512];
                                snprintf(btn, sizeof(btn), "<button label=\"%s\" onClick=\"CHOOSE:%s\" /> ", token, token);
                                strcat(g_completion_area, btn);
                                token = strtok(NULL, "  ");
                                count++;
                            }
                            free(copy);
                            free(matches);
                        } else {
                            snprintf(g_completion_area, sizeof(g_completion_area), "(no matches)");
                        }
                        state_changed = 1;
                    } else if (g_completion_mode) {
                        snprintf(g_completion_debug, sizeof(g_completion_debug), "No Star");
                        g_completion_mode = false;
                        g_completion_area[0] = '\0';
                        state_changed = 1;
                    } else {
                        snprintf(g_completion_debug, sizeof(g_completion_debug), "Waiting for Star");
                    }
                }

            }
        }

        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[1024];
                    while (fgets(line, sizeof(line), hf)) {
                        char *cmd_ptr = strstr(line, "SET_API:");
                        if (cmd_ptr) {
                            char *url = trim_str(cmd_ptr + 8);
                            strncpy(g_api_url, url, 255);
                            resolve_local_model();
                            snprintf(g_resp_area, sizeof(g_resp_area), "║ Connected to %-61.61s ║", g_api_url);
                            char *msg = NULL; if (asprintf(&msg, "Switched to %s", g_current_model) != -1) { snprintf(g_sys_msg, sizeof(g_sys_msg), "%s", msg); free(msg); }
                            state_changed = 1;
                        } else if ((cmd_ptr = strstr(line, "CHOOSE:"))) {
                            handle_choose_path(trim_str(cmd_ptr + 7));
                            state_changed = 1;
                        } else {
                            int key = 0;
                            char *bracket = strrchr(line, ']');
                            if (bracket) key = atoi(bracket + 1);
                            else key = atoi(line);

                            if (key == 10 || key == 13) { process_input_trigger(); state_changed = 1; }
                            else if (key == '1') { unlink(ctx_file); snprintf(g_resp_area, sizeof(g_resp_area), "║ Context cleared.                                                           ║"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Context Reset."); state_changed = 1; }
                            else if (key == '2') { load_apis(); update_menu_markup(); state_changed = 1; }
                            else if (key == '3') {
                                snprintf(g_ai_state, sizeof(g_ai_state), "SUMMARIZING"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Please wait...");
                                write_gui_state(); trigger_render();
                                char *r_args[] = {"projects/gem-api/ops/+x/json_state", "read", ctx_file, NULL}; char *context = run_tool(r_args[0], r_args, false);
                                char *tmp_prompt = "projects/gem-api/state/prompt.json"; char *tmp_llm = "projects/gem-api/state/llm_response.json"; char *tmp_content = "projects/gem-api/state/llm_content.json";
                                FILE *pf = fopen(tmp_prompt, "w"); 
                                bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
                                if (pf) { 
                                    if (is_llamacpp) {
                                        fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.\"},{\"role\":\"user\",\"content\":\"%s\"}]}", g_current_model, context); 
                                    } else {
                                        fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.\"},{\"role\":\"user\",\"content\":\"%s\"}]}", g_current_model, context); 
                                    }
                                    fclose(pf); 
                                }
                                free(context); char *body_arg = NULL; char *api_path = NULL;
                                if (is_llamacpp) {
                                    asprintf(&api_path, "%s/v1/chat/completions", g_api_url);
                                } else {
                                    asprintf(&api_path, "%s/api/chat", g_api_url);
                                }
                                if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
                                    char *curl_args[] = {"curl", "-s", "--max-time", "600", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                                    pid_t cpid = fork(); if (cpid == 0) { setpgid(0, 0); execvp("curl", curl_args); _exit(127); } if (cpid > 0) log_pid(cpid, "gem-api-summarize");
                                    waitpid(cpid, NULL, 0);
                                }
                                free(body_arg); free(api_path);
                                char *p_ext[] = {"projects/gem-api/ops/+x/json_parser", tmp_llm, "content", NULL}; char *content_json = run_tool(p_ext[0], p_ext, false);
                                if (content_json) {
                                    FILE *cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                                    char *p_sum[] = {"projects/gem-api/ops/+x/json_parser", tmp_content, "summary", NULL}; char *summary = run_tool(p_sum[0], p_sum, false);
                                    if (summary) {
                                        unlink(ctx_file); char *init_args[] = {"projects/gem-api/ops/+x/json_state", "append", ctx_file, "system", summary, NULL};
                                        run_tool(init_args[0], init_args, false); format_response(summary);
                                        snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); snprintf(g_sys_msg, sizeof(g_sys_msg), "Summary complete."); free(summary);
                                    }
                                    free(content_json);
                                }
                                state_changed = 1;
                            }
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        int old_pid = g_ai_pid;
        char old_state[64]; strncpy(old_state, g_ai_state, 63);

        check_ai_status(); 
        
        if (g_ai_pid > 0) {
            int cur = (int)(time(NULL) - g_thinking_start);
            if (cur != g_thinking_secs) {
                g_thinking_secs = cur;
                state_changed = 1;
            }
        } else {
            if (g_thinking_secs != 0) {
                g_thinking_secs = 0;
                state_changed = 1;
            }
        }
        
        if (old_pid > 0 && g_ai_pid <= 0) state_changed = 1; // AI finished or moved to ACTING
        if (strcmp(old_state, g_ai_state) != 0) state_changed = 1; // State string changed

        write_gui_state(); if (state_changed) trigger_render();
        usleep(100000); 
    }
    return 0;
}
