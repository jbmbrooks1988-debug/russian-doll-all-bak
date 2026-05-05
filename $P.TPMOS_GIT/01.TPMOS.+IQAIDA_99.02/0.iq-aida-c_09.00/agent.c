// agent.c - TPMOS CLI Agent Core (Strict JSON Escaping + Debug Fallback)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t spinner_active = 0;
static double spinner_start = 0;
static pthread_t spinner_tid;
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;

void sig_handler(int sig) { (void)sig; keep_running = 0; spinner_active = 0; }

void* spinner_thread(void* arg) {
    (void)arg;
    const char* chars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
    int idx = 0;
    while(spinner_active) {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        double elapsed = (ts.tv_sec + ts.tv_nsec/1e9) - spinner_start;
        fprintf(stderr, "\r\033[90m%c Thinking... (%.1fs)\033[0m", chars[idx % 10], elapsed);
        fflush(stderr); idx++; usleep(100000);
    }
    fprintf(stderr, "\r%50s\r", ""); fflush(stderr); return NULL;
}

void start_spinner() { spinner_active = 1; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); spinner_start = ts.tv_sec + ts.tv_nsec/1e9; pthread_create(&spinner_tid, NULL, spinner_thread, NULL); }
void stop_spinner() { spinner_active = 0; pthread_join(spinner_tid, NULL); }

static char* make_path(const char* base, const char* file) { char* out = NULL; if (asprintf(&out, "%s/%s", base, file) == -1) return NULL; return out; }


static char* run_tool(const char* tool, char* const args[]) {
    fprintf(stderr, "\033[90m[DEBUG] Forking: %s", tool);
    for (int i = 1; args && args[i]; i++) fprintf(stderr, " %s", args[i]);
    fprintf(stderr, "\033[0m\n");
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;
    pid_t pid = fork();
    if (pid == 0) { 
        close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO); 
        // Do NOT dup stderr here, so we can see it in our own terminal or ignore it
        close(pipefd[1]); 
        execvp(tool, args); 
        _exit(127); 
    }
    close(pipefd[1]);
    int status = 0; 
    char* output = malloc(ctx_limit);
    size_t total = 0;
    while (1) {
        char buf[1024];
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) break;
        if (total + n < ctx_limit) {
            memcpy(output + total, buf, n);
            total += n;
        }
    }
    output[total] = '\0';
    close(pipefd[0]);
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // Optional: handle error status
    }

    size_t len = strlen(output); while (len > 0 && (output[len-1] == '\n' || output[len-1] == '\r')) output[--len] = '\0';
    return output;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0); setvbuf(stdin, NULL, _IONBF, 0);
    struct sigaction sa; sa.sa_handler = sig_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = 0; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    mkdir("state", 0755); mkdir("config", 0755);

    // Load Context Configuration
    FILE* c_file = fopen("config/context.txt", "r");
    if (c_file) {
        char c_line[128];
        while (fgets(c_line, sizeof(c_line), c_file)) {
            if (strncmp(c_line, "limit=", 6) == 0) ctx_limit = atoll(c_line + 6);
            else if (strncmp(c_line, "divisor=", 8) == 0) ctx_divisor = atoi(c_line + 8);
        }
        fclose(c_file);
    }

    char* ctx_file = make_path("state", "context.json");
    char* yolo_file = make_path("config", "yolo.flag");
    char* tmp_llm = make_path("state", "llm_response.json");
    char* tmp_content = make_path("state", "llm_content.json");
    char* tmp_prompt = make_path("state", "prompt.json");
    char* tmp_args = make_path("state", "args.tmp");

    // Initialize or Sync context with system prompt
    FILE* fpers = fopen("config/persona.txt", "r");
    char* sys_msg = NULL;
    if (fpers) {
        fseek(fpers, 0, SEEK_END); long fsize = ftell(fpers); fseek(fpers, 0, SEEK_SET);
        sys_msg = malloc(fsize + 1);
        if (sys_msg) {
            size_t read_bytes = fread(sys_msg, 1, fsize, fpers);
            sys_msg[read_bytes] = '\0';
        }
        fclose(fpers);
    } else {
        sys_msg = strdup("You are Aida, an expert technical coding agent.");
    }

    int force_sync = 0;
    if (access(ctx_file, F_OK) != 0) {
        force_sync = 1;
    } else {
        // Simple check: does the file contain the sys_msg?
        char* read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
        char* current_ctx = run_tool(read_args[0], read_args);
        if (current_ctx) {
            if (sys_msg && !strstr(current_ctx, sys_msg)) {
                force_sync = 1;
            }
            free(current_ctx);
        }
    }

    if (force_sync) {
        unlink(ctx_file);
        char* init_args[] = {"./tools/json_state", "append", ctx_file, "system", sys_msg, NULL};
        run_tool(init_args[0], init_args);
    }
    free(sys_msg);

    printf("\033[94m[Aida] Agent Active.\033[0m (exit, /yolo, /clear)\n");
    
    char line[1024];
    while (keep_running) {
        // Calculate context stats (simple estimation)
        int pct = 0;
        char* read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
        char* full_ctx = run_tool(read_args[0], read_args);
        if (full_ctx) {
            pct = (strlen(full_ctx) / ctx_divisor); 
            if (pct > 100) pct = 100;
            free(full_ctx);
        }
        
        printf("[%s%d%%\033[0m] \033[92mAida >> \033[0m", pct < 75 ? "\033[92m" : "\033[91m", pct);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strcmp(line, "/yolo") == 0) {
            if (access(yolo_file, F_OK) == 0) { unlink(yolo_file); printf("YOLO: OFF\n"); }
            else { FILE* f = fopen(yolo_file, "w"); if(f){ fputs("1", f); fclose(f); } printf("YOLO: ON\n"); }
            continue;
        }
        if (strcmp(line, "/clear") == 0) {
            unlink(ctx_file);
            FILE* fpers = fopen("config/persona.txt", "r");
            char* sys_msg = NULL;
            if (fpers) {
                fseek(fpers, 0, SEEK_END); long fsize = ftell(fpers); fseek(fpers, 0, SEEK_SET);
                sys_msg = malloc(fsize + 1);
                if (sys_msg) {
                    size_t read_bytes = fread(sys_msg, 1, fsize, fpers);
                    sys_msg[read_bytes] = '\0';
                }
                fclose(fpers);
            }
 else {
                sys_msg = strdup("You are a technical coding agent.");
            }
            char* init_args[] = {"./tools/json_state", "append", ctx_file, "system", sys_msg, NULL};
            run_tool(init_args[0], init_args);
            free(sys_msg);
            printf("Memory cleared.\n"); continue;
        }
        if (strcmp(line, "/scan") == 0) {
            char* list_args[] = {"./tools/list_dir", ".", NULL};
            char* snapshot = run_tool(list_args[0], list_args);
            if (snapshot) {
                char* snapshot_msg = NULL;
                asprintf(&snapshot_msg, "[Context Snapshot]\n%s", snapshot);
                char* append_args[] = {"./tools/json_state", "append", ctx_file, "system", snapshot_msg, NULL};
                run_tool(append_args[0], append_args);
                printf("\033[90m%s\033[0m\n", snapshot_msg);
                printf("\033[90mContext snapshot injected. Aida now sees this structure.\033[0m\n");
                free(snapshot_msg); free(snapshot);
            }
            continue;
        }

        // 1. Append User Message to Context
        char* u_args[] = {"./tools/json_state", "append", ctx_file, "user", line, NULL};
        run_tool(u_args[0], u_args);

        while (keep_running) {
            // 2. Read Full Context for API Call
            char* ctx_read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
            char* context = run_tool(ctx_read_args[0], ctx_read_args);

            char model[256] = "llama3:latest";
            FILE* m_file = fopen("config/model.txt", "r");
            if (m_file) {
                if (fgets(model, sizeof(model), m_file)) {
                    model[strcspn(model, "\r\n")] = 0;
                }
                fclose(m_file);
            }

            FILE* pf = fopen(tmp_prompt, "w");
            if (!pf) { perror("fopen tmp_prompt"); free(context); break; }
            fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":%s}\n", 
                    model, context && strlen(context) > 2 ? context : "[]");
            fclose(pf); free(context);

            start_spinner();
            unlink(tmp_llm);
            pid_t curl_pid = fork();
            if (curl_pid == 0) {
                char* body_arg = NULL;
                if (asprintf(&body_arg, "@%s", tmp_prompt) != -1) {
                    char* curl_args[] = {"curl", "-s", "--max-time", "120", "-H", "Content-Type: application/json", "http://localhost:11434/api/chat", "-d", body_arg, "-o", tmp_llm, NULL};
                    execvp(curl_args[0], curl_args);
                    _exit(127);
                }
                _exit(127);
            }
            int curl_status = 0;
            while (1) {
                pid_t r = waitpid(curl_pid, &curl_status, WNOHANG);
                if (r == curl_pid) break;
                if (r == -1 && errno != EINTR) break;
                if (!keep_running) { kill(curl_pid, SIGTERM); break; }
                usleep(50000);
            }
            stop_spinner();

            if (!keep_running) break;
            if (!WIFEXITED(curl_status) || WEXITSTATUS(curl_status) != 0 || access(tmp_llm, F_OK) != 0) {
                fprintf(stderr, "\033[91m[ERR] LLM call failed (exit: %d, access: %d).\033[0m\n", 
                        WIFEXITED(curl_status) ? WEXITSTATUS(curl_status) : -1, access(tmp_llm, F_OK));
                break;
            }

            // 3. Extract message.content or tool_calls
            char* p_ext[] = {"./tools/json_parser", tmp_llm, "content", NULL};
            char* content_json = run_tool(p_ext[0], p_ext);
            
            char* p_calls[] = {"./tools/json_parser", tmp_llm, "tool_calls", NULL};
            char* tool_calls = run_tool(p_calls[0], p_calls);

            char* tool_name = NULL;
            char* args_json = NULL;

            if (tool_calls && strlen(tool_calls) > 2) {
                // Native tool calls found. Extract name and arguments from the first call.
                // Note: Our json_parser is simple, it will find the first "name" and "arguments" keys.
                FILE* fcalls = fopen("state/tool_calls.tmp", "w");
                if (fcalls) { fputs(tool_calls, fcalls); fclose(fcalls); }
                
                char* p_tn[] = {"./tools/json_parser", "state/tool_calls.tmp", "name", NULL};
                tool_name = run_tool(p_tn[0], p_tn);
                char* p_ta[] = {"./tools/json_parser", "state/tool_calls.tmp", "arguments", NULL};
                args_json = run_tool(p_ta[0], p_ta);
                unlink("state/tool_calls.tmp");
            } else if (content_json && strlen(content_json) > 0) {
                FILE* fcont = fopen(tmp_content, "w");
                if (fcont) { fputs(content_json, fcont); fclose(fcont); }
                
                char* p_tn[] = {"./tools/json_parser", tmp_content, "tool", NULL};
                tool_name = run_tool(p_tn[0], p_tn);
                char* p_ta[] = {"./tools/json_parser", tmp_content, "args", NULL};
                args_json = run_tool(p_ta[0], p_ta);
            }

            if (tool_name && strlen(tool_name) > 0) {
                FILE* fargs = fopen(tmp_args, "w");
                if (fargs) { fputs(args_json && strlen(args_json) > 0 ? args_json : "{}", fargs); fclose(fargs); }
                
                char* result = NULL;
                if (strcmp(tool_name, "exec_cmd") == 0 || strcmp(tool_name, "run_command") == 0) {
                    char* p_cmd[] = {"./tools/json_parser", tmp_args, "cmd", NULL};
                    char* command = run_tool(p_cmd[0], p_cmd);
                    if (!command || strlen(command) == 0) {
                        free(command);
                        char* p_cmd2[] = {"./tools/json_parser", tmp_args, "command", NULL};
                        command = run_tool(p_cmd2[0], p_cmd2);
                    }
                    if (command && strlen(command) > 0) {
                        char* exec_args[] = {"./tools/cmd_exec", command, NULL};
                        result = run_tool(exec_args[0], exec_args);
                        free(command);
                    }
                } else if (strcmp(tool_name, "read_file") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    if (path && strlen(path) > 0) {
                        char* tool_args[] = {"./tools/file_ops", "read", path, NULL};
                        result = run_tool(tool_args[0], tool_args);
                        free(path);
                    }
                } else if (strcmp(tool_name, "write_file") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* p_content[] = {"./tools/json_parser", tmp_args, "content", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    char* content = run_tool(p_content[0], p_content);
                    if (path && strlen(path) > 0 && content) {
                        char* tool_args[] = {"./tools/file_ops", "write", path, content, NULL};
                        result = run_tool(tool_args[0], tool_args);
                    }
                    free(path); free(content);
                } else if (strcmp(tool_name, "list_dir") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    char* tool_args[] = {"./tools/list_dir", path && strlen(path) > 0 ? path : ".", NULL};
                    result = run_tool(tool_args[0], tool_args);
                    free(path);
                } else if (strcmp(tool_name, "search_in_files") == 0) {
                    char* p_query[] = {"./tools/json_parser", tmp_args, "query", NULL};
                    char* query = run_tool(p_query[0], p_query);
                    if (query && strlen(query) > 0) {
                        char* tool_args[] = {"./tools/search_in_files", query, NULL};
                        result = run_tool(tool_args[0], tool_args);
                        free(query);
                    }
                } else if (strcmp(tool_name, "edit_file") == 0) {
                    char* p_path[] = {"./tools/json_parser", tmp_args, "path", NULL};
                    char* p_search[] = {"./tools/json_parser", tmp_args, "search", NULL};
                    char* p_replace[] = {"./tools/json_parser", tmp_args, "replace", NULL};
                    char* path = run_tool(p_path[0], p_path);
                    char* search = run_tool(p_search[0], p_search);
                    char* replace = run_tool(p_replace[0], p_replace);
                    if (path && search && replace && strlen(path) > 0) {
                        char* tool_args[] = {"./tools/edit_file", path, search, replace, NULL};
                        result = run_tool(tool_args[0], tool_args);
                    }
                    free(path); free(search); free(replace);
                } else if (strcmp(tool_name, "web_search") == 0) {
                    char* p_query[] = {"./tools/json_parser", tmp_args, "query", NULL};
                    char* query = run_tool(p_query[0], p_query);
                    if (query && strlen(query) > 0) {
                        char* tool_args[] = {"./tools/web_search", query, NULL};
                        result = run_tool(tool_args[0], tool_args);
                        free(query);
                    }
                }

                if (result) {
                    printf("\033[90m[Action: %s]\033[0m\n", tool_name);
                    printf("\033[92m>> %s\033[0m\n", result);
                    
                    // Native tool use wants the assistant message with tool_calls, then the tool result.
                    // For our simplified JSON in content, we append the content_json.
                    char* a_args[] = {"./tools/json_state", "append", ctx_file, "assistant", content_json && strlen(content_json) > 0 ? content_json : "{\"tool\": \"...\"}", NULL};
                    run_tool(a_args[0], a_args);
                    char* t_args[] = {"./tools/json_state", "append", ctx_file, "tool", result, NULL};
                    run_tool(t_args[0], t_args);
                    free(result);
                    free(tool_name); free(args_json); free(content_json); free(tool_calls);
                    continue; // Loop back for tool result
                }
            }

            char* p_resp[] = {"./tools/json_parser", tmp_content, "response", NULL};
            char* resp = run_tool(p_resp[0], p_resp);
            
            if (!resp || strlen(resp) == 0) {
                free(resp); resp = strdup(content_json && strlen(content_json) > 0 ? content_json : "(empty)");
            }

            printf("\033[92m>> %s\033[0m\n", resp);
            char* r_args[] = {"./tools/json_state", "append", ctx_file, "assistant", resp, NULL};
            run_tool(r_args[0], r_args);
            
            free(resp); free(tool_name); free(args_json); free(content_json); free(tool_calls);
            break; // Final response received
        }
    }

    free(ctx_file); free(yolo_file); free(tmp_llm); free(tmp_content); free(tmp_prompt); free(tmp_args);
    printf("\033[90mGraceful shutdown.\033[0m\n"); return 0;
}