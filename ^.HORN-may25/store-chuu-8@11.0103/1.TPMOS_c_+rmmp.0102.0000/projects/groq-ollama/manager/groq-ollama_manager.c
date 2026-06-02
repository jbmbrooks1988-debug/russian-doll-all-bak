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

#define PROJECT_ID "groq-ollama"
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

// Persistent Global UI State
static char g_ai_state[64] = "IDLE";
static char g_api_url[256] = "http://localhost:11434";
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

static void handle_sig(int s) { (void)s; g_shutdown = 1; }

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
            if (chdir("projects/groq-ollama/sandbox") != 0) _exit(1);
            if (strchr(tool_name, '/') || tool_name[0] == '.') { execvp(tool_name, args); }
            else { 
                if (asprintf(&full_path, "../ops/+x/%s", tool_name) != -1) { execvp(full_path, args); free(full_path); }
                execvp(tool_name, args); // Fallback to system path
            }
        } else {
            if (strchr(tool_name, '/') || tool_name[0] == '.') { execvp(tool_name, args); }
            else { 
                if (asprintf(&full_path, "projects/groq-ollama/ops/+x/%s", tool_name) != -1) { execvp(full_path, args); free(full_path); }
                execvp(tool_name, args); // Fallback to system path
            }
        }
        _exit(127);
    }
    if (pid > 0) log_pid(pid, "groq-ollama-tool");
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
    FILE *f = fopen("projects/groq-ollama/config/apis.txt", "r");
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

void audit_log(const char* user, const char* assistant) {
    char *path = "projects/groq-ollama/pieces/world_01/map_01/iqabel/memories/history.txt";
    mkdir("projects/groq-ollama/pieces/world_01/map_01/iqabel/memories", 0755);
    FILE *f = fopen(path, "a");
    if (f) {
        time_t now = time(NULL);
        char *ts = ctime(&now); ts[strlen(ts)-1] = '\0';
        fprintf(f, "[%s]\nUSER: %s\nAGENT: %s\n\n", ts, user, assistant);
        fclose(f);
    }
}

static void resolve_local_model(void) {
    bool is_gemini = strstr(g_api_url, "googleapis.com") != NULL;
    if (is_gemini) {
        strncpy(g_current_model, "gemini-1.5-flash", 255);
        return;
    }

    char* tags_file = "projects/groq-ollama/state/ollama_tags.json";
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
    char* find_args[] = {"projects/groq-ollama/ops/+x/json_parser", tags_file, key, NULL};
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

static void handle_gemini_stream(const char* chunk, char** acc_text, char** acc_func) {
    const char* p = chunk;
    
    // Robust pattern search: find "text" then skip to the start of the string
    const char* t = strstr(p, "\"text\"");
    if (t) {
        t = strchr(t, ':');
        if (t) {
            t = strchr(t, '"');
            if (t) {
                t++; // Start of content
                const char* start = t;
                while (*t && (*t != '"' || (*(t-1) == '\\'))) {
                    if (*t == '\\' && *(t+1)) t += 2;
                    else t++;
                }
                size_t len = t - start;
                size_t old_len = *acc_text ? strlen(*acc_text) : 0;
                *acc_text = realloc(*acc_text, old_len + len + 1);
                memcpy(*acc_text + old_len, start, len);
                (*acc_text)[old_len + len] = '\0';
                
                // Update projection in real-time
                char *unescaped = malloc(old_len + len + 1);
                char *s = *acc_text, *d = unescaped;
                while (*s) {
                    if (*s == '\\' && *(s+1)) {
                        s++;
                        if (*s == 'n') *d++ = '\n';
                        else if (*s == 't') *d++ = '\t';
                        else if (*s == '"') *d++ = '"';
                        else if (*s == '\\') *d++ = '\\';
                        else *d++ = *s;
                        s++;
                    } else *d++ = *s++;
                }
                *d = '\0';
                void format_response(const char* src);
                format_response(unescaped);
                free(unescaped);
                
                // Signal change via file for parent
                FILE *fs = fopen("projects/groq-ollama/state/stream_resp.txt", "w");
                if (fs) { fputs(*acc_text, fs); fclose(fs); }
                void trigger_render(void);
                trigger_render();
            }
        }
    }

    if (!*acc_func) {
        const char* f = strstr(p, "\"functionCall\"");
        if (f) {
            f = strchr(f, ':');
            if (f) {
                while (*f && *f != '{') f++;
                if (*f == '{') {
                    const char* start = f; int depth = 0;
                    while (*f) {
                        if (*f == '{') depth++; else if (*f == '}') depth--;
                        f++; if (depth == 0) break;
                    }
                    size_t len = f - start;
                    *acc_func = malloc(len + 1);
                    memcpy(*acc_func, start, len);
                    (*acc_func)[len] = '\0';
                }
            }
        }
    }
}

static char* get_gemini_key(void) {
    char *key = getenv("GEMINI_API_KEY");
    return key ? strdup(key) : NULL;
}

void start_ai_query(const char* input) {
    char *ctx_file = "projects/groq-ollama/state/context.json";
    char *tmp_prompt = "projects/groq-ollama/state/prompt.json";
    char *tmp_llm = "projects/groq-ollama/state/llm_response.json";
    char *tmp_content = "projects/groq-ollama/state/llm_content.json";
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
    bool is_gemini = strstr(g_api_url, "googleapis.com") != NULL;

    if (g_ai_pid > 0) return;
    unlink(tmp_llm); unlink(tmp_content);
    unlink("projects/groq-ollama/state/stream_resp.txt");
    struct stat st;
    if (stat(ctx_file, &st) != 0 || st.st_size < 10) {
        char *persona = read_full_file("config/persona.txt");
        if (persona && strlen(persona) > 5) {
            char *s_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", persona, NULL};
            run_tool(s_args[0], s_args, false);
            free(persona);
        } else {
            if (persona) free(persona);
            char *default_persona = "You are Aida, a technical agent. Respond in JSON with 'response' or 'tool' keys.";
            char *s_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", default_persona, NULL};
            run_tool(s_args[0], s_args, false);
        }
    }
    if (input) {
        if (g_pending_input) free(g_pending_input);
        g_pending_input = strdup(input);
        char *u_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "user", (char*)input, NULL};
        run_tool(u_args[0], u_args, false);
    }
    snprintf(g_ai_state, sizeof(g_ai_state), "THINKING");
    snprintf(g_fsm_state, sizeof(g_fsm_state), "THINKING");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Querying AI...");
    g_thinking_start = time(NULL);
    g_thinking_secs = 0;

    if (is_gemini) {
        char *api_key = get_gemini_key();
        if (!api_key) { snprintf(g_sys_msg, sizeof(g_sys_msg), "Error: GEMINI_API_KEY not found in ENV"); return; }
        
        char *build_args[] = {"projects/groq-ollama/ops/+x/gemini_payload_builder", ctx_file, tmp_prompt, NULL};
        run_tool(build_args[0], build_args, false);

        g_ai_pid = fork();
        if (g_ai_pid == 0) {
            setpgid(0, 0);
            char *curl_url = NULL; asprintf(&curl_url, "%s/v1beta/models/%s:streamGenerateContent?alt=sse", g_api_url, g_current_model);
            char *key_hdr = NULL; asprintf(&key_hdr, "x-goog-api-key: %s", api_key);
            char *curl_cmd = NULL; asprintf(&curl_cmd, "curl -s -N -H \"Content-Type: application/json\" -H \"%s\" -d @%s \"%s\" 2>&1", key_hdr, tmp_prompt, curl_url);
            
            FILE *stream_fp = popen(curl_cmd, "r");
            char *acc_text = NULL, *acc_func = NULL;
            if (stream_fp) {
                char line[8192];
                FILE *flog = fopen("projects/groq-ollama/state/gemini_stream.log", "w");
                while (fgets(line, sizeof(line), stream_fp)) {
                    if (flog) { fputs(line, flog); fflush(flog); }
                    if (strncmp(line, "data: ", 6) == 0) handle_gemini_stream(line + 6, &acc_text, &acc_func);
                    else if (strstr(line, "error") || strstr(line, "message")) {
                        // Likely a direct JSON error response instead of SSE
                        handle_gemini_stream(line, &acc_text, &acc_func);
                    }
                }
                if (flog) fclose(flog);
                pclose(stream_fp);
            }
            FILE *fllm = fopen(tmp_llm, "w");
            if (fllm) {
                // Flattened structure for the shallow json_parser
                fprintf(fllm, "{\"text\":\"%s\", \"functionCall\":%s}", 
                        acc_text ? acc_text : "", acc_func ? acc_func : "null");
                fclose(fllm);
            }
            _exit(0);
        }
        if (g_ai_pid > 0) log_pid(g_ai_pid, "groq-ollama-gemini");
        free(api_key);
    } else {
        char *r_args[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
        char *context = run_tool(r_args[0], r_args, false);
        FILE *pf = fopen(tmp_prompt, "w");
        if (pf) { 
            if (is_llamacpp) {
                fprintf(pf, "{\"model\":\"%s\",\"stream\":false,\"messages\":%s}\n", g_current_model, context ? context : "[]"); 
            } else {
                fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":%s}\n", g_current_model, context ? context : "[]"); 
            }
            fclose(pf); 
        }
        if (context) free(context);
        char *body_arg = NULL; char *api_path = NULL;
        
        if (is_llamacpp) {
            asprintf(&api_path, "%s/v1/chat/completions", g_api_url);
        } else {
            asprintf(&api_path, "%s/api/chat", g_api_url);
        }

        if (api_path && asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
            g_ai_pid = fork();
            if (g_ai_pid == 0) {
                setpgid(0, 0);
                char *curl_args[] = {"curl", "-v", "-s", "--max-time", "600", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                int fd = open("projects/groq-ollama/state/curl_debug.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
                execvp("curl", curl_args);
                _exit(127);
            }
            if (g_ai_pid > 0) log_pid(g_ai_pid, "groq-ollama-curl");
        }
        free(body_arg); free(api_path);
    }
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
    char *ctx_file = "projects/groq-ollama/state/context.json";
    char *tmp_llm = "projects/groq-ollama/state/llm_response.json";
    char *tmp_content = "projects/groq-ollama/state/llm_content.json";
    char *tmp_args = "projects/groq-ollama/state/args.tmp";
    bool is_llamacpp = strstr(g_api_url, ":8080") != NULL;
    bool is_gemini = strstr(g_api_url, "googleapis.com") != NULL;

    char *content_json = NULL;
    char *tool_calls = NULL;

    if (is_gemini) {
        char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "text", NULL};
        content_json = run_tool(p_ext[0], p_ext, false);
        char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "functionCall", NULL};
        tool_calls = run_tool(p_calls[0], p_calls, false);
    } else if (is_llamacpp) {
        // OpenAI-compat (llama.cpp) structure: choices[0].message.content or choices[0].message.tool_calls
        // Our json_parser is global/shallow, so we can try to grab "content" and "tool_calls" directly.
        char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "content", NULL};
        content_json = run_tool(p_ext[0], p_ext, false);
        char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "tool_calls", NULL};
        tool_calls = run_tool(p_calls[0], p_calls, false);
    } else {
        // Ollama structure: message.content or message.tool_calls
        char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "content", NULL};
        content_json = run_tool(p_ext[0], p_ext, false);
        char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "tool_calls", NULL};
        tool_calls = run_tool(p_calls[0], p_calls, false);
    }
    char *tool_name = NULL; char *args_json = NULL;
    if (tool_calls && strlen(tool_calls) > 2) {
        FILE *fcalls = fopen("projects/groq-ollama/state/tool_calls.tmp", "w");
        if (fcalls) { fputs(tool_calls, fcalls); fclose(fcalls); }
        char *p_tn[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "name", NULL};
        tool_name = run_tool(p_tn[0], p_tn, false);
        char *p_ta[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "arguments", NULL};
        args_json = run_tool(p_ta[0], p_ta, false);
        if (!args_json || strlen(args_json) == 0) {
            if (args_json) free(args_json);
            char *p_ta2[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "args", NULL};
            args_json = run_tool(p_ta2[0], p_ta2, false);
        }
        unlink("projects/groq-ollama/state/tool_calls.tmp");
    } else if (content_json && strlen(content_json) > 0) {
        FILE *fcont = fopen(tmp_content, "w");
        if (fcont) { fputs(content_json, fcont); fclose(fcont); }
        char *p_tn[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "tool", NULL};
        tool_name = run_tool(p_tn[0], p_tn, false);
        char *p_ta[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "args", NULL};
        args_json = run_tool(p_ta[0], p_ta, false);
    }
    if (tool_name && strlen(tool_name) > 0) {
        snprintf(g_ai_state, sizeof(g_ai_state), "ACTING");
        snprintf(g_fsm_state, sizeof(g_fsm_state), "%s", tool_name);
        snprintf(g_sys_msg, sizeof(g_sys_msg), "%s", tool_name);
        FILE *fargs = fopen(tmp_args, "w");
        if (fargs) { fputs(args_json ? args_json : "{}", fargs); fclose(fargs); }
        char *result = NULL;
        if (strcmp(tool_name, "read_file") == 0) {
            char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
            char *path = run_tool(p_p[0], p_p, false);
            if (path) { char *ta[] = {"file_ops", "read", path, NULL}; result = run_tool("file_ops", ta, true); free(path); }
        } else if (strcmp(tool_name, "write_file") == 0) {
            char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
            char *p_c[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "content", NULL};
            char *path = run_tool(p_p[0], p_p, false); char *cont = run_tool(p_c[0], p_c, false);
            if (path && cont) { char *ta[] = {"file_ops", "write", path, cont, NULL}; result = run_tool("file_ops", ta, true); }
            free(path); free(cont);
        } else if (strcmp(tool_name, "list_dir") == 0) {
            char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
            char *path = run_tool(p_p[0], p_p, false); char *ta[] = {"list_dir", path ? path : ".", NULL}; result = run_tool("list_dir", ta, true); free(path);
        } else if (strcmp(tool_name, "exec_cmd") == 0) {
            char *p_c[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "cmd", NULL};
            char *cmd = run_tool(p_c[0], p_c, false); if (cmd) { char *ta[] = {"cmd_exec", cmd, NULL}; result = run_tool("cmd_exec", ta, true); free(cmd); }
        }
        if (result) {
            char *a_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", content_json ? content_json : "{\"tool\":\"...\"}", NULL};
            run_tool(a_args[0], a_args, false);
            char *t_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "tool", result, NULL};
            run_tool(t_args[0], t_args, false);
            free(result);
        }
        free(tool_name); free(args_json); free(content_json); free(tool_calls);
        start_ai_query(NULL); return;
    }
    char *p_resp[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "response", NULL};
    char *resp = run_tool(p_resp[0], p_resp, false);
    if (!resp || strlen(resp) == 0) { 
        if (resp) free(resp); 
        resp = strdup((content_json && strlen(content_json) > 0) ? content_json : "(empty)"); 
    }
    
    // Final fallback for Gemini: if still empty, check the stream file
    if (is_gemini && (strcmp(resp, "(empty)") == 0 || strlen(resp) == 0)) {
        char *stream_data = read_full_file("projects/groq-ollama/state/stream_resp.txt");
        if (stream_data && strlen(stream_data) > 0) {
            free(resp);
            // Unescape stream data
            char *unescaped = malloc(strlen(stream_data) + 1);
            char *s = stream_data, *d = unescaped;
            while (*s) {
                if (*s == '\\' && *(s+1)) {
                    s++;
                    if (*s == 'n') *d++ = '\n';
                    else if (*s == 't') *d++ = '\t';
                    else if (*s == '"') *d++ = '"';
                    else if (*s == '\\') *d++ = '\\';
                    else *d++ = *s;
                    s++;
                } else *d++ = *s++;
            }
            *d = '\0';
            resp = unescaped;
            free(stream_data);
        }
    }
    audit_log(g_pending_input, resp); format_response(resp);
    snprintf(g_ai_state, sizeof(g_ai_state), "IDLE"); snprintf(g_fsm_state, sizeof(g_fsm_state), "IDLE");
    snprintf(g_sys_msg, sizeof(g_sys_msg), "Response received.");
    char *r_args_app[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", resp, NULL};
    run_tool(r_args_app[0], r_args_app, false);
    free(resp); free(tool_name); free(args_json); free(content_json); free(tool_calls);
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
    char *path = "projects/groq-ollama/manager/gui_state.txt"; char *line = NULL; size_t len = 0; FILE *f = fopen(path, "r");
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
    char *path = "projects/groq-ollama/manager/gui_state.txt"; char *existing_input = NULL; FILE *fr = fopen(path, "r");
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
        fprintf(f, "module_path=projects/groq-ollama/manager/+x/groq-ollama_manager.+x\n");
        fprintf(f, "active_layout_id=groq-ollama.chtpm\n");
        fprintf(f, "ai_state=%s\n", g_ai_state);
        fprintf(f, "active_api=%s\n", g_api_url);
        fprintf(f, "ai_status_line=%s\n", g_ai_status_line);
        fprintf(f, "ctx_pct=%s\n", g_ctx_pct);
        fprintf(f, "iqabel_fsm=%s\n", g_fsm_state);
        fprintf(f, "groq-ollama_response_area=%s\n", g_resp_area);
        fprintf(f, "groq-ollama_api_menu=%s\n", g_menu_area);
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
        char *path = "projects/groq-ollama/manager/gui_state.txt";
        FILE *f = fopen(path, "a"); if (f) { fprintf(f, "input_text=\n"); fclose(f); }
        start_ai_query(input);
    }
    if (input) free(input);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sig); signal(SIGTERM, handle_sig); setpgid(0, 0); log_pid(getpid(), "groq-ollama-manager");
    resolve_paths(argc > 1 ? argv[1] : NULL);
    if (chdir(project_root) != 0) perror("chdir project_root failed");
    mkdir("projects/groq-ollama/state", 0755); mkdir("projects/groq-ollama/sandbox", 0755); 
    load_apis(); update_menu_markup();
    resolve_local_model();
    char *hist_path = "pieces/apps/player_app/history.txt"; long last_pos = 0; struct stat st; if (stat(hist_path, &st) == 0) last_pos = st.st_size;
    while (!g_shutdown) {
        char *ctx_file = "projects/groq-ollama/state/context.json";
        int pct = 0; char *r_args_pct[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
        char *full_ctx = run_tool(r_args_pct[0], r_args_pct, false);
        if (full_ctx) { pct = (strlen(full_ctx) / ctx_divisor); if (pct > 100) pct = 100; free(full_ctx); }
        snprintf(g_ctx_pct, sizeof(g_ctx_pct), "%d%%", pct);
        int state_changed = 0;
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
                            // Debug log to manager_debug.log
                            FILE *df = fopen("manager_debug.log", "a");
                            if (df) { fprintf(df, "SET_API Triggered: %s\n", url); fclose(df); }
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
                                char *r_args[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL}; char *context = run_tool(r_args[0], r_args, false);
                                char *tmp_prompt = "projects/groq-ollama/state/prompt.json"; char *tmp_llm = "projects/groq-ollama/state/llm_response.json"; char *tmp_content = "projects/groq-ollama/state/llm_content.json";
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
                                    pid_t cpid = fork(); if (cpid == 0) { setpgid(0, 0); execvp("curl", curl_args); _exit(127); } if (cpid > 0) log_pid(cpid, "groq-ollama-summarize");
                                    waitpid(cpid, NULL, 0);
                                }
                                free(body_arg); free(api_path);
                                char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "content", NULL}; char *content_json = run_tool(p_ext[0], p_ext, false);
                                if (content_json) {
                                    FILE *cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                                    char *p_sum[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "summary", NULL}; char *summary = run_tool(p_sum[0], p_sum, false);
                                    if (summary) {
                                        unlink(ctx_file); char *init_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", summary, NULL};
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

        if (g_ai_pid > 0 && strcmp(g_ai_state, "THINKING") == 0) {
            char *stream_data = read_full_file("projects/groq-ollama/state/stream_resp.txt");
            if (stream_data) {
                // Unescape for display
                char *unescaped = malloc(strlen(stream_data) + 1);
                char *src = stream_data, *dst = unescaped;
                while (*src) {
                    if (*src == '\\' && *(src+1)) {
                        src++;
                        if (*src == 'n') *dst++ = '\n';
                        else if (*src == 't') *dst++ = '\t';
                        else if (*src == '"') *dst++ = '"';
                        else if (*src == '\\') *dst++ = '\\';
                        else *dst++ = *src;
                        src++;
                    } else *dst++ = *src++;
                }
                *dst = '\0';
                format_response(unescaped);
                free(unescaped); free(stream_data);
                state_changed = 1;
            }
        }

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
