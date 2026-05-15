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

#define PROJECT_ID "groq-ollama"
#define MAX_PATH 4096
#define MAX_LINE 1024

// Persistent Global UI State
static char g_ai_state[64] = "IDLE";
static char g_api_url[256] = "http://localhost:11434";
static char g_current_model[256] = "llama3:latest";
static char g_ctx_pct[16] = "0%";
static char g_fsm_state[64] = "IDLE";
static char g_resp_area[8192] = "║ Ready for input...                                                         ║";
static char g_sys_msg[256] = "Initialized.";

static char project_root[MAX_PATH] = ".";
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;
static volatile sig_atomic_t g_shutdown = 0;

static void handle_sig(int s) { (void)s; g_shutdown = 1; }

static char* run_tool(const char* tool_name, char* const args[], bool sandbox);

static void resolve_local_model(void) {
    char* tags_file = "projects/groq-ollama/state/ollama_tags.json";
    char* curl_args[] = {"curl", "-s", "http://localhost:11434/api/tags", "-o", tags_file, NULL};
    run_tool("curl", curl_args, false);
    
    char* find_args[] = {"projects/groq-ollama/ops/+x/json_parser", tags_file, "name", NULL};
    char* models = run_tool(find_args[0], find_args, false);
    
    if (models) {
        char* copy = strdup(models);
        char* token = strtok(copy, "  ");
        int found = 0;
        while (token) {
            if (strstr(token, "groq")) {
                strncpy(g_current_model, token, 255);
                found = 1; break;
            }
            token = strtok(NULL, "  ");
        }
        if (!found) {
            char* first = strtok(strdup(models), "  ");
            if (first) strncpy(g_current_model, first, 255);
        }
        free(copy);
        free(models);
    }
}

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

void resolve_paths(const char* hint) {
    char kvp_path[MAX_PATH];
    snprintf(kvp_path, sizeof(kvp_path), "%s/pieces/locations/location_kvp", hint ? hint : ".");
    
    FILE *kvp = fopen(kvp_path, "r");
    if (kvp) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), kvp)) {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char *k = trim_str(line);
                char *v = trim_str(eq + 1);
                if (strcmp(k, "project_root") == 0) {
                    snprintf(project_root, sizeof(project_root), "%s", v);
                }
            }
        }
        fclose(kvp);
    } else {
        if (hint) snprintf(project_root, sizeof(project_root), "%s", hint);
    }
}

void trigger_render(void) {
    char *path = "pieces/display/frame_changed.txt";
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "M\n"); fclose(f); }
}

void write_gui_state(void) {
    char *path = "projects/groq-ollama/manager/gui_state.txt";
    
    char *existing_input = NULL;
    FILE *fr = fopen(path, "r");
    if (fr) {
        char *line = NULL; size_t len = 0;
        while (getline(&line, &len, fr) != -1) {
            if (strncmp(line, "input_text=", 11) == 0) {
                existing_input = strdup(line + 11);
                if (existing_input[strlen(existing_input)-1] == '\n') existing_input[strlen(existing_input)-1] = '\0';
            }
        }
        free(line); fclose(fr);
    }

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "module_path=projects/groq-ollama/manager/+x/groq-ollama_manager.+x\n");
        fprintf(f, "active_layout_id=groq-ollama.chtpm\n");
        fprintf(f, "ai_state=%s\n", g_ai_state);
        fprintf(f, "active_api=%s\n", g_api_url);
        fprintf(f, "ctx_pct=%s\n", g_ctx_pct);
        fprintf(f, "iqabel_fsm=%s\n", g_fsm_state);
        fprintf(f, "groq-ollama_response_area=%s\n", g_resp_area);
        fprintf(f, "sys_msg=%s\n", g_sys_msg);
        if (existing_input) fprintf(f, "input_text=%s\n", existing_input);
        fclose(f);
    }
    free(existing_input);
}

char* run_tool(const char* tool_name, char* const args[], bool sandbox) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]); dup2(pipefd[1], STDOUT_FILENO); close(pipefd[1]);
        
        char *full_path = NULL;
        if (sandbox) {
            // CONFINE TO SANDBOX
            if (chdir("projects/groq-ollama/sandbox") != 0) {
                perror("chdir to sandbox failed");
                _exit(1);
            }
            // Resolve tool path from inside sandbox (tools are in ../ops/+x/)
            if (strchr(tool_name, '/') || tool_name[0] == '.') {
                execvp(tool_name, args);
            } else {
                asprintf(&full_path, "../ops/+x/%s", tool_name);
                execvp(full_path, args);
            }
        } else {
            // RUN IN TPMOS ROOT
            if (strchr(tool_name, '/') || tool_name[0] == '.') {
                execvp(tool_name, args);
            } else {
                asprintf(&full_path, "projects/groq-ollama/ops/+x/%s", tool_name);
                execvp(full_path, args);
            }
        }
        _exit(127);
    }
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

char* get_gui_var(const char* id) {
    char *path = "projects/groq-ollama/manager/gui_state.txt";
    char *line = NULL;
    size_t len = 0;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, id, strlen(id)) == 0 && line[strlen(id)] == '=') {
            char *val = strdup(line + strlen(id) + 1);
            if (val[strlen(val)-1] == '\n') val[strlen(val)-1] = '\0';
            fclose(f); free(line); return val;
        }
    }
    fclose(f); free(line); return NULL;
}

void format_response(const char* src) {
    char formatted[8192] = "";
    char line_buf[512];
    const char* p = src;
    
    while (*p && strlen(formatted) < 7000) {
        int len = 0;
        while (p[len] && p[len] != '\n' && len < 74) len++;
        
        char wrap[75];
        strncpy(wrap, p, len); wrap[len] = '\0';
        snprintf(line_buf, sizeof(line_buf), "║ %-74.74s ║\\n", wrap);
        strcat(formatted, line_buf);
        
        p += len;
        if (*p == '\n') p++;
    }
    if (strlen(formatted) > 2) formatted[strlen(formatted)-2] = '\0';
    strncpy(g_resp_area, formatted, sizeof(g_resp_area)-1);
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

void process_input(void) {
    char *ctx_file = "projects/groq-ollama/state/context.json";
    char *tmp_llm = "projects/groq-ollama/state/llm_response.json";
    char *tmp_content = "projects/groq-ollama/state/llm_content.json";
    char *tmp_prompt = "projects/groq-ollama/state/prompt.json";
    char *tmp_args = "projects/groq-ollama/state/args.tmp";

    char *input = get_gui_var("input_text");
    if (input && strlen(input) > 0) {
        char *path = "projects/groq-ollama/manager/gui_state.txt";
        FILE *f = fopen(path, "a"); if (f) { fprintf(f, "input_text=\n"); fclose(f); }

        strcpy(g_resp_area, "║ Agent is thinking...                                                       ║");
        strcpy(g_ai_state, "THINKING");
        strcpy(g_fsm_state, "THINKING");
        strcpy(g_sys_msg, "Querying AI...");
        write_gui_state(); trigger_render();

        char *u_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "user", input, NULL};
        run_tool(u_args[0], u_args, false);
        
        int multi_turn = 1;
        while (multi_turn && !g_shutdown) {
            int pct = 0;
            char *r_args_pct[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
            char *full_ctx = run_tool(r_args_pct[0], r_args_pct, false);
            if (full_ctx) { pct = (strlen(full_ctx) / ctx_divisor); if (pct > 100) pct = 100; free(full_ctx); }
            snprintf(g_ctx_pct, sizeof(g_ctx_pct), "%d%%", pct);

            write_gui_state(); trigger_render();

            char *r_args[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
            char *context = run_tool(r_args[0], r_args, false);
            char model[256]; strncpy(model, g_current_model, 255);

            FILE *pf = fopen(tmp_prompt, "w");
            if (pf) { fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":%s}\n", model, context); fclose(pf); }
            free(context);

            char *body_arg = NULL; asprintf(&body_arg, "@%s", tmp_prompt);
            char *api_path = NULL; asprintf(&api_path, "%s/api/chat", g_api_url);
            char *curl_args[] = {"curl", "-s", "--max-time", "60", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
            
            pid_t cpid = fork(); if (cpid == 0) { execvp("curl", curl_args); _exit(127); } waitpid(cpid, NULL, 0);
            free(body_arg); free(api_path);

            char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "content", NULL};
            char *content_json = run_tool(p_ext[0], p_ext, false);
            char *p_calls[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "tool_calls", NULL};
            char *tool_calls = run_tool(p_calls[0], p_calls, false);

            char *tool_name = NULL; char *args_json = NULL;
            if (tool_calls && strlen(tool_calls) > 2) {
                FILE *fcalls = fopen("projects/groq-ollama/state/tool_calls.tmp", "w");
                if (fcalls) { fputs(tool_calls, fcalls); fclose(fcalls); }
                char *p_tn[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "name", NULL};
                tool_name = run_tool(p_tn[0], p_tn, false);
                char *p_ta[] = {"projects/groq-ollama/ops/+x/json_parser", "projects/groq-ollama/state/tool_calls.tmp", "arguments", NULL};
                args_json = run_tool(p_ta[0], p_ta, false);
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
                strcpy(g_ai_state, "ACTING");
                strncpy(g_fsm_state, tool_name, sizeof(g_fsm_state)-1);
                strncpy(g_sys_msg, tool_name, sizeof(g_sys_msg)-1);
                write_gui_state(); trigger_render();

                FILE *fargs = fopen(tmp_args, "w");
                if (fargs) { fputs(args_json ? args_json : "{}", fargs); fclose(fargs); }
                
                char *result = NULL;
                // AI AGENDA TOOLS ARE RUN IN SANDBOX
                if (strcmp(tool_name, "read_file") == 0) {
                    char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
                    char *path = run_tool(p_p[0], p_p, false);
                    if (path) { char *ta[] = {"file_ops", "read", path, NULL}; result = run_tool("file_ops", ta, true); free(path); }
                } else if (strcmp(tool_name, "write_file") == 0) {
                     char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
                     char *p_c[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "content", NULL};
                     char *path = run_tool(p_p[0], p_p, false);
                     char *cont = run_tool(p_c[0], p_c, false);
                     if (path && cont) { char *ta[] = {"file_ops", "write", path, cont, NULL}; result = run_tool("file_ops", ta, true); }
                     free(path); free(cont);
                } else if (strcmp(tool_name, "list_dir") == 0) {
                     char *p_p[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "path", NULL};
                     char *path = run_tool(p_p[0], p_p, false);
                     char *ta[] = {"list_dir", path ? path : ".", NULL}; result = run_tool("list_dir", ta, true);
                     free(path);
                } else if (strcmp(tool_name, "exec_cmd") == 0) {
                     char *p_c[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_args, "cmd", NULL};
                     char *cmd = run_tool(p_c[0], p_c, false);
                     if (cmd) { char *ta[] = {"cmd_exec", cmd, NULL}; result = run_tool("cmd_exec", ta, true); free(cmd); }
                }

                if (result) {
                    char *a_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", content_json ? content_json : "{\"tool\":\"...\"}", NULL};
                    run_tool(a_args[0], a_args, false);
                    char *t_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "tool", result, NULL};
                    run_tool(t_args[0], t_args, false);
                    free(result);
                }
                free(tool_name); free(args_json); free(content_json); free(tool_calls);
                continue;
            }

            char *p_resp[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "response", NULL};
            char *resp = run_tool(p_resp[0], p_resp, false);
            if (!resp || strlen(resp) == 0) { free(resp); resp = strdup(content_json ? content_json : "(empty)"); }
            
            audit_log(input, resp);
            format_response(resp);
            strcpy(g_ai_state, "IDLE");
            strcpy(g_fsm_state, "IDLE");
            strcpy(g_sys_msg, "Response received.");
            write_gui_state(); trigger_render();
            
            char *r_args_app[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "assistant", resp, NULL};
            run_tool(r_args_app[0], r_args_app, false);
            
            free(resp); free(tool_name); free(args_json); free(content_json); free(tool_calls);
            multi_turn = 0;
        }
    }
    free(input);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);
    setpgid(0, 0);
    resolve_paths(argc > 1 ? argv[1] : NULL);
    if (chdir(project_root) != 0) {
        perror("chdir project_root failed");
    }
    mkdir("projects/groq-ollama/state", 0755);
    mkdir("projects/groq-ollama/sandbox", 0755);
    resolve_local_model();

    char *hist_path = "pieces/apps/player_app/history.txt";
    long last_pos = 0; struct stat st;
    if (stat(hist_path, &st) == 0) last_pos = st.st_size;

    while (!g_shutdown) {
        char *ctx_file = "projects/groq-ollama/state/context.json";

        int pct = 0;
        char *r_args_pct[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
        char *full_ctx = run_tool(r_args_pct[0], r_args_pct, false);
        if (full_ctx) { pct = (strlen(full_ctx) / ctx_divisor); if (pct > 100) pct = 100; free(full_ctx); }
        snprintf(g_ctx_pct, sizeof(g_ctx_pct), "%d%%", pct);

        int state_changed = 0;
        if (stat(hist_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(hist_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    int key;
                    while (fscanf(hf, "%d", &key) == 1) {
                        if (key == 10 || key == 13) { process_input(); state_changed = 1; }
                        else if (key == '1') {
                            unlink(ctx_file);
                            strcpy(g_resp_area, "║ Context cleared.                                                           ║");
                            strcpy(g_sys_msg, "Context Reset.");
                            state_changed = 1;
                        } else if (key == '2') {
                            FILE *af = fopen("projects/groq-ollama/config/apis.txt", "r");
                            if (af) {
                                char line[256];
                                while (fgets(line, sizeof(line), af)) {
                                    char *p = strchr(line, '|');
                                    if (p) {
                                        char *url = p + 1;
                                        url[strcspn(url, "\r\n")] = 0;
                                        if (strcmp(url, g_api_url) != 0) { strcpy(g_api_url, url); break; }
                                    }
                                }
                                fclose(af);
                            }
                            strcpy(g_resp_area, "║ API switched.                                                              ║");
                            strcpy(g_sys_msg, "API Changed.");
                            state_changed = 1;
                        } else if (key == '3') {
                            strcpy(g_ai_state, "SUMMARIZING"); strcpy(g_sys_msg, "Please wait...");
                            write_gui_state(); trigger_render();
                            char *r_args[] = {"projects/groq-ollama/ops/+x/json_state", "read", ctx_file, NULL};
                            char *context = run_tool(r_args[0], r_args, false);
                            char *tmp_prompt = "projects/groq-ollama/state/prompt.json";
                            char *tmp_llm = "projects/groq-ollama/state/llm_response.json";
                            char *tmp_content = "projects/groq-ollama/state/llm_content.json";
                            FILE *pf = fopen(tmp_prompt, "w");
                            if (pf) { fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.\"},{\"role\":\"user\",\"content\":\"%s\"}]}", g_current_model, context); fclose(pf); }
                            free(context);
                            char *body_arg = NULL; asprintf(&body_arg, "@%s", tmp_prompt);
                            char *api_path = NULL; asprintf(&api_path, "%s/api/chat", g_api_url);
                            char *curl_args[] = {"curl", "-s", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                            pid_t cpid = fork(); if (cpid == 0) { execvp("curl", curl_args); _exit(127); } waitpid(cpid, NULL, 0);
                            free(body_arg); free(api_path);
                            char *p_ext[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_llm, "content", NULL};
                            char *content_json = run_tool(p_ext[0], p_ext, false);
                            if (content_json) {
                                FILE *cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                                char *p_sum[] = {"projects/groq-ollama/ops/+x/json_parser", tmp_content, "summary", NULL};
                                char *summary = run_tool(p_sum[0], p_sum, false);
                                if (summary) {
                                    unlink(ctx_file);
                                    char *init_args[] = {"projects/groq-ollama/ops/+x/json_state", "append", ctx_file, "system", summary, NULL};
                                    run_tool(init_args[0], init_args, false);
                                    format_response(summary);
                                    strcpy(g_ai_state, "IDLE"); strcpy(g_sys_msg, "Summary complete.");
                                    free(summary);
                                }
                                free(content_json);
                            }
                            state_changed = 1;
                        }
                    }
                    last_pos = ftell(hf); fclose(hf);
                }
            } else if (st.st_size < last_pos) last_pos = 0;
        }
        
        write_gui_state();
        if (state_changed) trigger_render();
        usleep(100000); 
    }
    return 0;
}
