#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define HOST "gateway.discord.gg"
#define REST_HOST "discord.com"
#define PORT 443
#define KEYS_FILE "#.bot_keys.txt"
#define STATE_FILE "state.txt"
#define LOG_FILE "main_log.txt"
#define RECEIVE_LOG "receive_log.txt"
#define PID_FILE "pids.txt"

// Global variable to store child PIDs
pid_t child_pids[15]; // Increased from 14 to 15 to accommodate search.+x
int child_pid_count = 0;

char TOKEN[128] = "";
char FIXED_CHANNEL_ID[32] = "";

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) { 
        fprintf(f, "[%ld] %s\n", time(NULL), msg); 
        fclose(f); 
    }
    printf("[%ld] %s\n", time(NULL), msg);
}

void log_pid(const char* process_name, pid_t pid) {
    FILE* f = fopen(PID_FILE, "a");
    if (f) {
        fprintf(f, "[%ld] %s: %d\n", time(NULL), process_name, pid);
        fclose(f);
    }
    if (pid > 0) {
        child_pids[child_pid_count++] = pid;
    }
}

void signal_handler(int sig) {
    log_message("Received termination signal, cleaning up child processes...");
    for (int i = 0; i < child_pid_count; i++) {
        if (child_pids[i] > 0 && kill(child_pids[i], SIGTERM) == 0) {
            waitpid(child_pids[i], NULL, 0);
            char log[256];
            snprintf(log, sizeof(log), "Terminated child process PID %d", child_pids[i]);
            log_message(log);
        }
    }
    exit(0);
}

void init_openssl() { 
    SSL_load_error_strings(); 
    OpenSSL_add_ssl_algorithms(); 
}

SSL_CTX* create_context() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { 
        log_message("Unable to create SSL context"); 
        exit(1); 
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    return ctx;
}

void base64_encode(char* dest, const unsigned char* src, size_t len) {
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, j = 0;
    for (i = 0; i < len - 2; i += 3) {
        dest[j++] = b64[(src[i] >> 2) & 0x3F];
        dest[j++] = b64[((src[i] & 0x3) << 4) | ((src[i + 1] >> 4) & 0xF)];
        dest[j++] = b64[((src[i + 1] & 0xF) << 2) | ((src[i + 2] >> 6) & 0x3)];
        dest[j++] = b64[src[i + 2] & 0x3F];
    }
    if (len % 3) {
        if (len % 3 == 1) {
            dest[j++] = b64[(src[i] >> 2) & 0x3F];
            dest[j++] = b64[(src[i] & 0x3) << 4];
            j += 2; // Skip padding
        } else {
            dest[j++] = b64[(src[i] >> 2) & 0x3F];
            dest[j++] = b64[((src[i] & 0x3) << 4) | ((src[i + 1] >> 4) & 0xF)];
            dest[j++] = b64[(src[i + 1] & 0xF) << 2];
            j += 1; // Skip one padding
        }
    }
    dest[j] = '\0';
}

void update_state(const char* key, const char* value) {
    FILE* f = fopen(STATE_FILE, "a");
    if (f) { 
        fprintf(f, "%s=%s\n", key, value); 
        fclose(f); 
    }
}

int read_state(const char* key, char* value, size_t len) {
    FILE* f = fopen(STATE_FILE, "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, key)) {
            sscanf(line, "%*[^=]=%s", value);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int read_keys() {
    FILE* f = fopen(KEYS_FILE, "r");
    if (!f) {
        log_message("Failed to open keys file");
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "#define TOKEN")) {
            sscanf(line, "#define TOKEN \"%[^\"]\"", TOKEN);
        } else if (strstr(line, "#define FIXED_CHANNEL_ID")) {
            sscanf(line, "#define FIXED_CHANNEL_ID \"%[^\"]\"", FIXED_CHANNEL_ID);
        }
    }
    fclose(f);
    if (strlen(TOKEN) == 0 || strlen(FIXED_CHANNEL_ID) == 0) {
        log_message("Failed to read TOKEN or FIXED_CHANNEL_ID from keys file");
        return 0;
    }
    return 1;
}

pid_t start_receive_process(char* session_id, int sequence) {
    pid_t pid = fork();
    if (pid == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "./+x/receive.+x");
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        log_message("Failed to start receive.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for receive.+x");
        return -1;
    }
    log_message("Started receive.+x in background");
    log_pid("receive.+x", pid);
    return pid;
}

pid_t start_hunt_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/hunt.+x", NULL);
        log_message("Failed to start hunt.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for hunt.+x");
        return -1;
    }
    log_message("Started hunt.+x in background");
    log_pid("hunt.+x", pid);
    return pid;
}

pid_t start_chem_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/chem.+x", NULL);
        log_message("Failed to start chem.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for chem.+x");
        return -1;
    }
    log_message("Started chem.+x in background");
    log_pid("chem.+x", pid);
    return pid;
}

pid_t start_craft_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/craft.+x", NULL);
        log_message("Failed to start craft.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for craft.+x");
        return -1;
    }
    log_message("Started craft.+x in background");
    log_pid("craft.+x", pid);
    return pid;
}

pid_t start_games_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/games.+x", NULL);
        log_message("Failed to start games.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for games.+x");
        return -1;
    }
    log_message("Started games.+x in background");
    log_pid("games.+x", pid);
    return pid;
}

pid_t start_search_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/search.+x", NULL);
        log_message("Failed to start search.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for search.+x");
        return -1;
    }
    log_message("Started search.+x in background");
    log_pid("search.+x", pid);
    return pid;
}

pid_t start_ai_ollama_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/ai_ollama_module.+x", NULL);
        log_message("Failed to start ai_ollama_module.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for ai_ollama_module.+x");
        return -1;
    }
    log_message("Started ai_ollama_module.+x in background");
    log_pid("ai_ollama_module.+x", pid);
    return pid;
}

pid_t start_llama2_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/llama2_module.+x", NULL);
        log_message("Failed to start llama2_module.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for llama2_module.+x");
        return -1;
    }
    log_message("Started llama2_module.+x in background");
    log_pid("llama2_module.+x", pid);
    return pid;
}

pid_t start_bible_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/bible.+x", NULL);
        log_message("Failed to start bible.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for bible.+x");
        return -1;
    }
    log_message("Started bible.+x in background");
    log_pid("bible.+x", pid);
    return pid;
}

pid_t start_users_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/users.+x", NULL);
        log_message("Failed to start users.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for users.+x");
        return -1;
    }
    log_message("Started users.+x in background");
    log_pid("users.+x", pid);
    return pid;
}

pid_t start_buy_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/buy.+x", NULL);
        log_message("Failed to start buy.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for buy.+x");
        return -1;
    }
    log_message("Started buy.+x in background");
    log_pid("buy.+x", pid);
    return pid;
}

pid_t start_sell_process() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", "./+x/sell.+x", NULL);
        log_message("Failed to start sell.+x");
        exit(1);
    } else if (pid < 0) {
        log_message("Fork failed for sell.+x");
        return -1;
    }
    log_message("Started sell.+x in background");
    log_pid("sell.+x", pid);
    return pid;
}

int is_process_running(pid_t pid) {
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0;
}

int check_receive_log_for_errors() {
    FILE* f = fopen(RECEIVE_LOG, "r");
    if (!f) return 0;
    
    char line[256];
    int error_count = 0;
    time_t now = time(NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size == 0) {
        fclose(f);
        return 0;
    }
    
    long pos = size;
    int lines = 0;
    while (pos > 0 && lines < 5) {
        pos--;
        fseek(f, pos, SEEK_SET);
        if (fgetc(f) == '\n') lines++;
    }
    if (pos > 0) fseek(f, pos + 1, SEEK_SET);
    
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "Connection lost")) {
            error_count++;
        }
    }
    fclose(f);
    return error_count >= 3;
}

int main() {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    log_message("Starting main.+x");
    log_pid("main.+x", getpid());

    if (!read_keys()) {
        log_message("Failed to initialize, exiting...");
        return 1;
    }
    char log[256];
    snprintf(log, sizeof(log), "Loaded TOKEN: %s, CHANNEL_ID: %s", TOKEN, FIXED_CHANNEL_ID);
    log_message(log);

    init_openssl();
    SSL_CTX* gateway_ctx = create_context();

    int gateway_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent* he = gethostbyname(HOST);
    struct sockaddr_in gateway_addr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    memcpy(&gateway_addr.sin_addr, he->h_addr_list[0], he->h_length);
    connect(gateway_sock, (struct sockaddr*)&gateway_addr, sizeof(gateway_addr));

    SSL* gateway_ssl = SSL_new(gateway_ctx);
    SSL_set_fd(gateway_ssl, gateway_sock);
    SSL_set_tlsext_host_name(gateway_ssl, HOST);
    SSL_connect(gateway_ssl);

    unsigned char key[16];
    RAND_bytes(key, 16);
    char ws_key[25];
    base64_encode(ws_key, key, 16);

    char handshake[1024];
    snprintf(handshake, sizeof(handshake),
             "GET /?v=10&encoding=json HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nUser-Agent: DiscordBot/1.0\r\n\r\n",
             HOST, ws_key);
    SSL_write(gateway_ssl, handshake, strlen(handshake));

    char buffer[16384];
    int bytes = SSL_read(gateway_ssl, buffer, sizeof(buffer) - 1);
    buffer[bytes] = '\0';

    char session_id[128] = "";
    int sequence = 0;
    char value[32];
    if (read_state("session_id", session_id, sizeof(session_id))) {
        log_message("Loaded session_id from state");
    }
    if (read_state("sequence", value, sizeof(value))) {
        sequence = atoi(value);
        log_message("Loaded sequence from state");
    }

    if (strstr(buffer, "101 Switching Protocols")) {
        char identify[256];
        snprintf(identify, sizeof(identify),
                 "{\"op\":2,\"d\":{\"token\":\"%s\",\"intents\":33281,\"properties\":{\"os\":\"linux\",\"browser\":\"custom\",\"device\":\"custom\"}}}",
                 TOKEN);
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "./+x/send.+x ws \"%s\"", identify);
        printf("Executing: %s\n", cmd);
        log_message(cmd);
        int ret = system(cmd);
        if (ret != 0) {
            char log[256];
            snprintf(log, sizeof(log), "send.+x (ws) failed with return code %d", ret);
            log_message(log);
        }

        pid_t receive_pid = start_receive_process(session_id, sequence);
        pid_t hunt_pid = start_hunt_process();
        pid_t chem_pid = start_chem_process();
        pid_t craft_pid = start_craft_process();
        pid_t games_pid = start_games_process();
        pid_t search_pid = start_search_process();
        pid_t ai_ollama_pid = start_ai_ollama_process();
        pid_t llama2_pid = start_llama2_process();
        pid_t bible_pid = start_bible_process();
        pid_t users_pid = start_users_process();
        pid_t buy_pid = start_buy_process();
        pid_t sell_pid = start_sell_process();

        printf("Enter a message to send manually (or 'quit' to exit):\n");
        while (1) {
            if (!is_process_running(receive_pid) || check_receive_log_for_errors()) {
                log_message("receive.+x not running or stuck, restarting...");
                if (receive_pid > 0) {
                    kill(receive_pid, SIGTERM);
                    waitpid(receive_pid, NULL, 0);
                }
                if (check_receive_log_for_errors()) {
                    session_id[0] = '\0';
                    sequence = 0;
                    update_state("session_id", "");
                    update_state("sequence", "0");
                }
                receive_pid = start_receive_process(session_id, sequence);
            }
            if (!is_process_running(hunt_pid)) {
                log_message("hunt.+x not running, restarting...");
                if (hunt_pid > 0) {
                    kill(hunt_pid, SIGTERM);
                    waitpid(hunt_pid, NULL, 0);
                }
                hunt_pid = start_hunt_process();
            }
            if (!is_process_running(chem_pid)) {
                log_message("chem.+x not running, restarting...");
                if (chem_pid > 0) {
                    kill(chem_pid, SIGTERM);
                    waitpid(chem_pid, NULL, 0);
                }
                chem_pid = start_chem_process();
            }
            if (!is_process_running(craft_pid)) {
                log_message("craft.+x not running, restarting...");
                if (craft_pid > 0) {
                    kill(craft_pid, SIGTERM);
                    waitpid(craft_pid, NULL, 0);
                }
                craft_pid = start_craft_process();
            }
            if (!is_process_running(games_pid)) {
                log_message("games.+x not running, restarting...");
                if (games_pid > 0) {
                    kill(games_pid, SIGTERM);
                    waitpid(games_pid, NULL, 0);
                }
                games_pid = start_games_process();
            }
            if (!is_process_running(search_pid)) {
                log_message("search.+x not running, restarting...");
                if (search_pid > 0) {
                    kill(search_pid, SIGTERM);
                    waitpid(search_pid, NULL, 0);
                }
                search_pid = start_search_process();
            }
            if (!is_process_running(ai_ollama_pid)) {
                log_message("ai_ollama_module.+x not running, restarting...");
                if (ai_ollama_pid > 0) {
                    kill(ai_ollama_pid, SIGTERM);
                    waitpid(ai_ollama_pid, NULL, 0);
                }
                ai_ollama_pid = start_ai_ollama_process();
            }
            if (!is_process_running(llama2_pid)) {
                log_message("llama2_module.+x not running, restarting...");
                if (llama2_pid > 0) {
                    kill(llama2_pid, SIGTERM);
                    waitpid(llama2_pid, NULL, 0);
                }
                llama2_pid = start_llama2_process();
            }
            if (!is_process_running(bible_pid)) {
                log_message("bible.+x not running, restarting...");
                if (bible_pid > 0) {
                    kill(bible_pid, SIGTERM);
                    waitpid(bible_pid, NULL, 0);
                }
                bible_pid = start_bible_process();
            }
            if (!is_process_running(users_pid)) {
                log_message("users.+x not running, restarting...");
                if (users_pid > 0) {
                    kill(users_pid, SIGTERM);
                    waitpid(users_pid, NULL, 0);
                }
                users_pid = start_users_process();
            }
            if (!is_process_running(buy_pid)) {
                log_message("buy.+x not running, restarting...");
                if (buy_pid > 0) {
                    kill(buy_pid, SIGTERM);
                    waitpid(buy_pid, NULL, 0);
                }
                buy_pid = start_buy_process();
            }
            if (!is_process_running(sell_pid)) {
                log_message("sell.+x not running, restarting...");
                if (sell_pid > 0) {
                    kill(sell_pid, SIGTERM);
                    waitpid(sell_pid, NULL, 0);
                }
                sell_pid = start_sell_process();
            }

            char cli_input[1024];
            if (fgets(cli_input, sizeof(cli_input), stdin)) {
                cli_input[strcspn(cli_input, "\n")] = 0;
                if (strcmp(cli_input, "quit") == 0) {
                    if (receive_pid > 0) {
                        kill(receive_pid, SIGTERM);
                        waitpid(receive_pid, NULL, 0);
                    }
                    if (hunt_pid > 0) {
                        kill(hunt_pid, SIGTERM);
                        waitpid(hunt_pid, NULL, 0);
                    }
                    if (chem_pid > 0) {
                        kill(chem_pid, SIGTERM);
                        waitpid(chem_pid, NULL, 0);
                    }
                    if (craft_pid > 0) {
                        kill(craft_pid, SIGTERM);
                        waitpid(craft_pid, NULL, 0);
                    }
                    if (games_pid > 0) {
                        kill(games_pid, SIGTERM);
                        waitpid(games_pid, NULL, 0);
                    }
                    if (search_pid > 0) {
                        kill(search_pid, SIGTERM);
                        waitpid(search_pid, NULL, 0);
                    }
                    if (ai_ollama_pid > 0) {
                        kill(ai_ollama_pid, SIGTERM);
                        waitpid(ai_ollama_pid, NULL, 0);
                    }
                    if (llama2_pid > 0) {
                        kill(llama2_pid, SIGTERM);
                        waitpid(llama2_pid, NULL, 0);
                    }
                    if (bible_pid > 0) {
                        kill(bible_pid, SIGTERM);
                        waitpid(bible_pid, NULL, 0);
                    }
                    if (users_pid > 0) {
                        kill(users_pid, SIGTERM);
                        waitpid(users_pid, NULL, 0);
                    }
                    if (buy_pid > 0) {
                        kill(buy_pid, SIGTERM);
                        waitpid(buy_pid, NULL, 0);
                    }
                    if (sell_pid > 0) {
                        kill(sell_pid, SIGTERM);
                        waitpid(sell_pid, NULL, 0);
                    }
                    break;
                }
                if (strlen(cli_input) > 0) {
                    FILE* fp = fopen("send_queue.txt", "w");
                    if (fp) {
                        fprintf(fp, "%s|%s", FIXED_CHANNEL_ID, cli_input);
                        fclose(fp);
                    }
                    snprintf(cmd, sizeof(cmd), "./+x/send.+x");
                    printf("Executing: %s\n", cmd);
                    log_message(cmd);
                    ret = system(cmd);
                    if (ret != 0) {
                        char log[256];
                        snprintf(log, sizeof(log), "send.+x (rest) failed with return code %d", ret);
                        log_message(log);
                    }
                }
            }

            int heartbeat_interval = 0;
            if (read_state("heartbeat_interval", value, sizeof(value))) {
                heartbeat_interval = atoi(value);
            }
            if (read_state("sequence", value, sizeof(value))) {
                sequence = atoi(value);
            }
            time_t last_heartbeat = 0;
            if (read_state("last_heartbeat", value, sizeof(value))) {
                last_heartbeat = atol(value);
            }

            time_t now = time(NULL);
            if (heartbeat_interval > 0 && (now - last_heartbeat) >= (heartbeat_interval / 1000)) {
                snprintf(cmd, sizeof(cmd), "./+x/send.+x ws \"{\\\"op\\\":1,\\\"d\\\":%d}\"", sequence ? sequence : -1);
                printf("Executing: %s\n", cmd);
                log_message(cmd);
                ret = system(cmd);
                if (ret != 0) {
                    char log[256];
                    snprintf(log, sizeof(log), "send.+x (heartbeat) failed with return code %d", ret);
                    log_message(log);
                }
                char ts[32];
                snprintf(ts, sizeof(ts), "%ld", now);
                update_state("last_heartbeat", ts);
            }

            sleep(1);
        }
    }

    SSL_shutdown(gateway_ssl);
    SSL_free(gateway_ssl);
    close(gateway_sock);
    SSL_CTX_free(gateway_ctx);
    EVP_cleanup();
    return 0;
}
