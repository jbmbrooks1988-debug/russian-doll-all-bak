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
#include <stdarg.h>

#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>

static volatile sig_atomic_t action_interrupted = 0;
static volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t spinner_active = 0;
static double spinner_start = 0;
static pthread_t spinner_tid;
static size_t ctx_limit = 65536;
static int ctx_divisor = 300;
static FILE* debug_fp = NULL;
static int feat_completion = 1;
static int feat_summarize = 1;
static struct termios orig_termios;
static char current_api_url[256] = "http://localhost:11434";

void sig_handler(int sig) { 
    (void)sig; 
    action_interrupted = 1; 
    spinner_active = 0; 
}

void debug_print(const char* fmt, ...) {
    if (!debug_fp) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(debug_fp, fmt, args);
    va_end(args);
    fflush(debug_fp);
}

static void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static char* run_tool(const char* tool, char* const args[]) {
    debug_print("[DEBUG] Forking: %s", tool);
    for (int i = 1; args && args[i]; i++) debug_print(" %s", args[i]);
    debug_print("\n");
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

static void clear_dropdown() {
    // Move down 1, clear line, move back up 1, CR
    printf("\033[s\033[1B\r\033[K\033[u");
    fflush(stdout);
}

static void refresh_ui(int pct, const char* line, const char* suggestions, int selected_idx, const char* sub_suggestions) {
    // 1. Move to start of prompt line and clear everything below it
    printf("\r\033[J"); 
    
    // 2. Draw Prompt and Line
    printf("[%s%d%%\033[0m] \033[92mAida >> \033[0m%s", 
           pct < 75 ? "\033[92m" : "\033[91m", pct, line);
    
    // 3. Draw Dropdown if active
    if (suggestions && strlen(suggestions) > 0) {
        printf("\033[s"); // Save cursor position at end of prompt line
        printf("\n\033[90m"); // Move to next line, set gray
        printf("[tab] "); // Hint for cycling
        
        char* copy = strdup(suggestions);
        char* token = strtok(copy, "  ");
        int i = 0;
        while (token) {
            if (i == selected_idx) printf("\033[7m %s \033[27m ", token);
            else printf(" %s  ", token);
            token = strtok(NULL, "  ");
            i++;
            if (i > 8) { printf("..."); break; }
        }
        
        if (sub_suggestions && strlen(sub_suggestions) > 0) {
            printf("\n\033[90m       ↳ %s", sub_suggestions);
        }
        
        printf("\033[0m\033[u"); // Restore cursor to prompt line
    }
    fflush(stdout);
}

static char* get_line_interactive(int pct) {
    char* line = calloc(1024, 1);
    size_t len = 0;
    char* current_suggestions = NULL;
    int selected_idx = -1;
    
    refresh_ui(pct, line, NULL, -1, NULL);

    enable_raw_mode();
    while (keep_running) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;
        
        if (c == 27) { // Escape sequence
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;
            continue;
        }

        if (c == 13 || c == 10) { // Enter
            if (selected_idx != -1 && current_suggestions) {
                // COMMIT SELECTION
                char* copy = strdup(current_suggestions);
                char* token = strtok(copy, "  ");
                for (int i = 0; i < selected_idx; i++) token = strtok(NULL, "  ");
                if (token) {
                    char* last_word = strrchr(line, ' ');
                    if (!last_word) last_word = line; else last_word++;
                    if (last_word[0] == '@') {
                        line[last_word - line + 1] = '\0';
                        strcat(line, token);
                        len = strlen(line);
                    }
                }
                free(copy);
                selected_idx = -1;
                if (current_suggestions) { free(current_suggestions); current_suggestions = NULL; }
                refresh_ui(pct, line, NULL, -1, NULL);
                continue; // Stay in the interactive loop
            }
            // ELSE SEND MESSAGE
            printf("\r\033[J[%s%d%%\033[0m] \033[92mAida >> \033[0m%s\n", pct < 75 ? "\033[92m" : "\033[91m", pct, line);
            break;
        } else if (c == 127 || c == 8) { // Backspace
            if (len > 0) {
                line[--len] = '\0';
                selected_idx = -1;
            }
        } else if (c == 9) { // Tab
            if (feat_completion && current_suggestions && strlen(current_suggestions) > 0) {
                int count = 0;
                char* p = current_suggestions;
                while (p) {
                    count++;
                    p = strstr(p, "  ");
                    if (p) p += 2;
                }
                selected_idx = (selected_idx + 1) % count;
            }
        } else if (c == 3) { // Ctrl+C
            line[0] = '\0'; len = 0;
            printf("\n");
            refresh_ui(pct, line, NULL, -1, NULL);
            continue;
        } else if (c == 4) { // Ctrl+D
            if (len == 0) { free(line); disable_raw_mode(); return NULL; }
        } else if (c >= 32 && c <= 126) {
            if (c == '/' && selected_idx != -1 && current_suggestions) {
                // AUTO-COMMIT ON SLASH
                char* copy = strdup(current_suggestions);
                char* token = strtok(copy, "  ");
                for (int i = 0; i < selected_idx; i++) token = strtok(NULL, "  ");
                if (token) {
                    char* last_word = strrchr(line, ' ');
                    if (!last_word) last_word = line; else last_word++;
                    if (last_word[0] == '@') {
                        line[last_word - line + 1] = '\0';
                        strcat(line, token);
                        if (line[strlen(line)-1] != '/') strcat(line, "/");
                        len = strlen(line);
                    }
                }
                free(copy);
                selected_idx = -1;
            } else {
                if (len < 1023) {
                    line[len++] = c; line[len] = '\0';
                    selected_idx = -1;
                }
            }
        }

        // Pulse and Refresh
        char* sub_suggestions = NULL;
        if (feat_completion) {
            char* last_word = strrchr(line, ' ');
            if (!last_word) last_word = line; else last_word++;
            
            if (last_word[0] == '@') {
                char* prefix = last_word + 1;
                char* comp_args[] = {"./tools/complete_path", prefix, NULL};
                char* new_sug = run_tool(comp_args[0], comp_args);
                if (current_suggestions) free(current_suggestions);
                current_suggestions = new_sug;

                if (selected_idx != -1 && current_suggestions) {
                    char* copy = strdup(current_suggestions);
                    char* tok = strtok(copy, "  ");
                    for (int i = 0; i < selected_idx; i++) tok = strtok(NULL, "  ");
                    if (tok && tok[strlen(tok)-1] == '/') {
                        char* sub_args[] = {"./tools/complete_path", tok, NULL};
                        sub_suggestions = run_tool(sub_args[0], sub_args);
                    }
                    free(copy);
                }
            } else {
                if (current_suggestions) { free(current_suggestions); current_suggestions = NULL; }
                selected_idx = -1;
            }
        }
        refresh_ui(pct, line, current_suggestions, selected_idx, sub_suggestions);
        if (sub_suggestions) free(sub_suggestions);
    }
    if (current_suggestions) free(current_suggestions);
    disable_raw_mode();
    return line;
}

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

static void frame_clear() {
    struct winsize w;
    int lines = 40;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0) lines = w.ws_row;
    for (int i = 0; i < lines; i++) printf("\n");
    printf("\033[H\033[2J");
    fflush(stdout);
}

static void log_session_frame(const char* user, const char* assistant) {
    FILE* f = fopen("debug/session.log", "a");
    if (!f) return;
    fprintf(f, "[USER]\n%s\n\n[AIDA]\n%s\n\n========================================\n\n", user, assistant);
    fclose(f);
}

static char* make_path(const char* base, const char* file) { char* out = NULL; if (asprintf(&out, "%s/%s", base, file) == -1) return NULL; return out; }

static char current_model[256] = "llama3:latest";

static void resolve_local_model() {
    char* tags_file = "state/ollama_tags.json";
    char* curl_args[] = {"curl", "-s", "http://localhost:11434/api/tags", "-o", tags_file, NULL};
    run_tool(curl_args[0], curl_args);
    
    // Attempt to find a 'groq' model first
    char* find_args[] = {"./tools/json_parser", tags_file, "name", NULL};
    char* models = run_tool(find_args[0], find_args);
    
    if (models) {
        char* copy = strdup(models);
        char* token = strtok(copy, "  ");
        int found = 0;
        while (token) {
            if (strstr(token, "groq")) {
                strncpy(current_model, token, 255);
                found = 1; break;
            }
            token = strtok(NULL, "  ");
        }
        if (!found) {
            // Fallback to first available model if no groq found
            char* first = strtok(strdup(models), "  ");
            if (first) strncpy(current_model, first, 255);
        }
        free(copy);
        free(models);
    }
    printf("\033[90m[System] Resolved Model: %s\033[0m\n", current_model);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0); setvbuf(stdin, NULL, _IONBF, 0);
    struct sigaction sa; sa.sa_handler = sig_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = 0; sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    mkdir("state", 0755); mkdir("config", 0755); mkdir("debug", 0755);

    debug_fp = fopen("debug/debug.txt", "w");
    if (!debug_fp) perror("Warning: Could not open debug/debug.txt");

    FILE* sf_init = fopen("debug/session.log", "w");
    if (sf_init) fclose(sf_init);

    // Resolve Model Dynamically
    resolve_local_model();

    // Load API Selection
    FILE* asf = fopen("state/active_api.txt", "r");
    if (asf) { 
        if (fgets(current_api_url, sizeof(current_api_url), asf)) 
            current_api_url[strcspn(current_api_url, "\r\n")] = 0; 
        fclose(asf); 
    }

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

    // Load Features Configuration
    FILE* f_file = fopen("config/features.txt", "r");
    if (f_file) {
        char f_line[128];
        while (fgets(f_line, sizeof(f_line), f_file)) {
            if (strncmp(f_line, "completion=off", 14) == 0) feat_completion = 0;
            else if (strncmp(f_line, "summarize=off", 13) == 0) feat_summarize = 0;
        }
        fclose(f_file);
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
    
    char* last_resp = NULL;
    char* line = NULL;
    while (keep_running) {
        action_interrupted = 0;
        // Calculate context stats (simple estimation)
        int pct = 0;
        char* read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
        char* full_ctx = run_tool(read_args[0], read_args);
        if (full_ctx) {
            pct = (strlen(full_ctx) / ctx_divisor); 
            if (pct > 100) pct = 100;
            free(full_ctx);
        }
        
        frame_clear();
        if (last_resp) {
            printf("\033[90mLast Response:\033[0m\n\033[92m>> %s\033[0m\n\n", last_resp);
        }

        if (line) free(line);
        line = get_line_interactive(pct);
        if (!line) break;
        if (strlen(line) == 0) continue;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "/exit") == 0) {
            keep_running = 0; break;
        }
        if (strcmp(line, "/yolo") == 0) {
            if (access(yolo_file, F_OK) == 0) { unlink(yolo_file); printf("YOLO: OFF\n"); log_session_frame("/yolo", "OFF"); }
            else { FILE* f = fopen(yolo_file, "w"); if(f){ fputs("1", f); fclose(f); } printf("YOLO: ON\n"); log_session_frame("/yolo", "ON"); }
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
                if (asprintf(&snapshot_msg, "[Context Snapshot]\n%s", snapshot) != -1) {
                    char* append_args[] = {"./tools/json_state", "append", ctx_file, "system", snapshot_msg, NULL};
                    run_tool(append_args[0], append_args);
                    printf("\033[90m%s\033[0m\n", snapshot_msg);
                    printf("\033[90mContext snapshot injected. Aida now sees this structure.\033[0m\n");
                    log_session_frame("/scan", snapshot_msg);
                    free(snapshot_msg);
                }
                free(snapshot);
            }
            continue;
        }
        if (strcmp(line, "/summarize") == 0) {
            if (!feat_summarize) { printf("Summarization is disabled in config/features.txt\n"); continue; }
            printf("Summarizing conversation...\n");
            log_session_frame("/summarize", "Summarizing conversation...");
            char* read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
            char* context = run_tool(read_args[0], read_args);
            
            char model[256];
            strncpy(model, current_model, 255);

            FILE* pf = fopen(tmp_prompt, "w");
            if (pf) {
                fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":[{\"role\":\"system\",\"content\":\"Condense the following conversation into a concise summary of facts, decisions, and current project state. Respond ONLY with a JSON object containing a 'summary' key.\"},{\"role\":\"user\",\"content\":\"%s\"}]}", model, context);
                fclose(pf);
            }
            free(context);

            start_spinner();
            char* body_arg = NULL; 
            char* api_path = NULL;
            if (asprintf(&body_arg, "@%s", tmp_prompt) != -1 && asprintf(&api_path, "%s/api/chat", current_api_url) != -1) {
                char* full_curl[] = {"curl", "-s", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                run_tool(full_curl[0], full_curl);
            }
            stop_spinner(); free(body_arg); free(api_path);

            char* p_ext[] = {"./tools/json_parser", tmp_llm, "content", NULL};
            char* content_json = run_tool(p_ext[0], p_ext);
            if (content_json) {
                FILE* cf = fopen(tmp_content, "w"); if (cf) { fputs(content_json, cf); fclose(cf); }
                char* p_sum[] = {"./tools/json_parser", tmp_content, "summary", NULL};
                char* summary = run_tool(p_sum[0], p_sum);
                if (summary) {
                    unlink(ctx_file);
                    char* init_args[] = {"./tools/json_state", "append", ctx_file, "system", summary, NULL};
                    run_tool(init_args[0], init_args);
                    printf("\033[92m>> Summary: %s\033[0m\n", summary);
                    if (last_resp) free(last_resp);
                    last_resp = strdup(summary);
                    log_session_frame("system", summary);
                    free(summary);
                }
                free(content_json);
            }
            continue;
        }
        if (strcmp(line, "/api") == 0) {
            FILE* af = fopen("config/apis.txt", "r");
            if (!af) { printf("Error: config/apis.txt not found.\n"); continue; }
            char names[1024] = {0};
            char urls[4][256];
            char al[256];
            int ac = 0;
            while (fgets(al, sizeof(al), af) && ac < 4) {
                char* p = strchr(al, '|');
                if (p) {
                    *p = '\0';
                    strcat(names, al); strcat(names, "  ");
                    strcpy(urls[ac++], p + 1);
                    urls[ac-1][strcspn(urls[ac-1], "\r\n")] = 0;
                }
            }
            fclose(af);
            printf("Select API (Tab to cycle, Enter to select):\n");
            int sel = 0;
            enable_raw_mode();
            while (1) {
                refresh_ui(pct, "Select", names, sel, NULL);
                char c; read(STDIN_FILENO, &c, 1);
                if (c == 9) sel = (sel + 1) % ac;
                else if (c == 13 || c == 10) {
                    strcpy(current_api_url, urls[sel]);
                    FILE* sf = fopen("state/active_api.txt", "w");
                    if (sf) { fputs(current_api_url, sf); fclose(sf); }
                    clear_dropdown();
                    printf("\033[94m>> API set to: %s\033[0m\n", current_api_url);
                    log_session_frame("/api", current_api_url);
                    break;
                }
 else if (c == 3) { clear_dropdown(); break; }
            }
            disable_raw_mode();
            continue;
        }

        // 1. Append User Message to Context
        char* u_args[] = {"./tools/json_state", "append", ctx_file, "user", line, NULL};
        run_tool(u_args[0], u_args);

        while (keep_running && !action_interrupted) {
            // 2. Read Full Context for API Call
            char* ctx_read_args[] = {"./tools/json_state", "read", ctx_file, NULL};
            char* context = run_tool(ctx_read_args[0], ctx_read_args);

            char model[256];
            strncpy(model, current_model, 255);

            FILE* pf = fopen(tmp_prompt, "w");
            if (!pf) { perror("fopen tmp_prompt"); free(context); break; }
            fprintf(pf, "{\"model\":\"%s\",\"format\":\"json\",\"stream\":false,\"messages\":%s}\n", 
                    model, context && strlen(context) > 2 ? context : "[]");
            fclose(pf); free(context);

            start_spinner();
            unlink(tmp_llm);
                        // API Key Handling: Groq API requires an Authorization header.
            // In a production environment, fetch this from env or secure config.
            // Example: char* groq_api_key = getenv("GROQ_API_KEY");
            // if (groq_api_key) { ... add "-H", "Authorization: Bearer ...", ... }
            pid_t curl_pid = fork();
            if (curl_pid == 0) {
                char* body_arg = NULL;
                char* api_path = NULL;
                if (asprintf(&body_arg, "@%s", tmp_prompt) != -1 && asprintf(&api_path, "%s/api/chat", current_api_url) != -1) {
                    char* curl_args[] = {"curl", "-s", "--max-time", "120", "-H", "Content-Type: application/json", api_path, "-d", body_arg, "-o", tmp_llm, NULL};
                    execvp(curl_args[0], curl_args);
                }
                _exit(127);
            }
            int curl_status = 0;
            while (1) {
                pid_t r = waitpid(curl_pid, &curl_status, WNOHANG);
                if (r == curl_pid) break;
                if (r == -1 && errno != EINTR) break;
                if (action_interrupted) { kill(curl_pid, SIGTERM); break; }
                usleep(50000);
            }
            stop_spinner();

            if (action_interrupted) {
                printf("\n\033[91m[Action Cancelled]\033[0m\n");
                break;
            }

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
            
            if (last_resp) free(last_resp);
            last_resp = strdup(resp);
            log_session_frame(line, resp);

            free(resp); free(tool_name); free(args_json); free(content_json); free(tool_calls);
            break; // Final response received
        }
    }

    free(ctx_file); free(yolo_file); free(tmp_llm); free(tmp_content); free(tmp_prompt); free(tmp_args);
    if (last_resp) free(last_resp);
    if (debug_fp) fclose(debug_fp);
    printf("\033[90mGraceful shutdown.\033[0m\n"); return 0;
}