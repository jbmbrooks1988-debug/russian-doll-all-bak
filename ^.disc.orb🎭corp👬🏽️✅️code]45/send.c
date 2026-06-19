#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define HOST "gateway.discord.gg"
#define REST_HOST "discord.com"
#define PORT 443
#define KEYS_FILE "#.bot_keys.txt"
#define BUFFER_SIZE 16384
#define MAX_PAYLOAD_SIZE 65536
#define LOG_FILE "/+x/send_log.txt"

char TOKEN[128] = "";
char FIXED_CHANNEL_ID[32] = "";

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) { fprintf(f, "[%ld] %s\n", time(NULL), msg); fclose(f); }
}

void init_openssl() { SSL_load_error_strings(); OpenSSL_add_ssl_algorithms(); }

SSL_CTX* create_context() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { log_message("Unable to create SSL context"); exit(1); }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    return ctx;
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

void send_websocket_frame(SSL* ssl, const char* payload) {
    size_t len = strlen(payload);
    if (len > MAX_PAYLOAD_SIZE - 8) { log_message("Payload too large"); return; }
    unsigned char frame[MAX_PAYLOAD_SIZE];
    frame[0] = 0x81;
    int header_len;
    unsigned char* mask_key;

    if (len < 126) {
        frame[1] = 0x80 | (unsigned char)len;
        header_len = 6;
        mask_key = frame + 2;
    } else if (len <= 65535) {
        frame[1] = 0x80 | 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        header_len = 8;
        mask_key = frame + 4;
    } else {
        log_message("Payload exceeds 64KB limit");
        return;
    }

    RAND_bytes(mask_key, 4);
    unsigned char* masked_payload = frame + header_len;
    for (size_t i = 0; i < len; i++) {
        masked_payload[i] = payload[i] ^ mask_key[i % 4];
    }
    int written = SSL_write(ssl, frame, header_len + len);
    char log[256];
    if (written <= 0) {
        snprintf(log, sizeof(log), "Failed to send WebSocket frame: %d", SSL_get_error(ssl, written));
        log_message(log);
    } else {
        snprintf(log, sizeof(log), "Sent WebSocket frame: %s", payload);
        log_message(log);
    }
}

void send_rest_message(SSL* ssl, const char* channel_id, const char* content) {
    char request[2048];
    char body[1024];
    snprintf(body, sizeof(body), "{\"content\":\"%s\"}", content);
    size_t body_len = strlen(body);
    snprintf(request, sizeof(request),
             "POST /api/v10/channels/%s/messages HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Authorization: Bot %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "User-Agent: DiscordBot/1.0\r\n"
             "\r\n"
             "%s",
             channel_id, REST_HOST, TOKEN, body_len, body);
    int written = SSL_write(ssl, request, strlen(request));
    char log[256];
    if (written <= 0) {
        snprintf(log, sizeof(log), "Failed to send REST message: %d", SSL_get_error(ssl, written));
        log_message(log);
    } else {
        snprintf(log, sizeof(log), "Sent REST message: %s", body);
        log_message(log);
        char response[4096];
        int bytes = SSL_read(ssl, response, sizeof(response) - 1);
        if (bytes > 0) {
            response[bytes] = '\0';
            snprintf(log, sizeof(log), "REST response: %s", response);
            log_message(log);
        }
    }
}

int main(int argc, char* argv[]) {
    if (!read_keys()) {
        log_message("Failed to initialize, exiting...");
        return 1;
    }
    char log[256];
    snprintf(log, sizeof(log), "Loaded TOKEN: %s, CHANNEL_ID: %s", TOKEN, FIXED_CHANNEL_ID);
    log_message(log);

    FILE* fp = fopen("send_queue.txt", "r");
    if (!fp) {
        log_message("No send queue");
        return 0;
    }

    char line[1024];
    if (fgets(line, sizeof(line), fp)) {
        char* channel_id = strtok(line, "|");
        char* message = strtok(NULL, "\n");
        if (channel_id && message) {
            char log[256];
            snprintf(log, sizeof(log), "Processing queue: %s|%s", channel_id, message);
            log_message(log);

            init_openssl();
            SSL_CTX* ctx = create_context();

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct hostent* he = gethostbyname(REST_HOST);
            struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
            memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) <0) {
                log_message("Connection failed");
                return 1;
            }

            SSL* ssl = SSL_new(ctx);
            SSL_set_fd(ssl, sock);
            SSL_set_tlsext_host_name(ssl, REST_HOST);
            if (SSL_connect(ssl) <= 0) {
                log_message("SSL handshake failed");
                return 1;
            }

            send_rest_message(ssl, channel_id, message);

            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(sock);
            SSL_CTX_free(ctx);
            EVP_cleanup();
        }
    }
    fclose(fp);

    fp = fopen("send_queue.txt", "w");
    if (fp) fclose(fp);

    return 0;
}
