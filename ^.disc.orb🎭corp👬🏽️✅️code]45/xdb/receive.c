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
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

#define HOST "gateway.discord.gg"
#define PORT 443
#define KEYS_FILE "#.bot_keys.txt"
#define BUFFER_SIZE 65536
#define MAX_PAYLOAD_SIZE 131072
#define CONTENT_SIZE 4096
#define STATE_FILE "state.txt"
#define LOG_FILE "receive_log.txt"

char TOKEN[128] = "";
char FIXED_CHANNEL_ID[32] = "";

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) { fprintf(f, "[%ld] %s\n", time(NULL), msg); fflush(f); fclose(f); }
    printf("[%ld] %s\n", time(NULL), msg);
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
            dest[j++] = '=';
            dest[j++] = '=';
        } else {
            dest[j++] = b64[(src[i] >> 2) & 0x3F];
            dest[j++] = b64[((src[i] & 0x3) << 4) | ((src[i + 1] >> 4) & 0xF)];
            dest[j++] = b64[(src[i + 1] & 0xF) << 2];
            dest[j++] = '=';
        }
    }
    dest[j] = '\0';
}

void update_state(const char* key, const char* value) {
    FILE* f = fopen(STATE_FILE, "a");
    if (f) { fprintf(f, "%s=%s\n", key, value); fflush(f); fclose(f); }
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

unsigned int simple_hash(const char* str) {
    unsigned int hash = 0;
    for (int i = 0; str[i]; i++) {
        hash = hash * 31 + str[i];
    }
    return hash % 1000000;
}

void send_websocket_frame(SSL* ssl, const char* payload) {
    size_t len = strlen(payload);
    if (len >= MAX_PAYLOAD_SIZE - 8) { log_message("Payload too large"); return; }
    unsigned char frame[MAX_PAYLOAD_SIZE];
    frame[0] = 0x81;
    int offset;
    unsigned char mask_key[4];
    RAND_bytes(mask_key, 4);

    if (len < 126) {
        frame[1] = 0x80 | len;
        offset = 2;
    } else {
        frame[1] = 0x80 | 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        offset = 4;
    }

    memcpy(frame + offset, mask_key, 4);
    offset += 4;
    for (size_t i = 0; i < len; i++) {
        frame[offset + i] = payload[i] ^ mask_key[i % 4];
    }

    int frame_len = offset + len;
    int written = SSL_write(ssl, frame, frame_len);
    char log[256];
    if (written <= 0) {
        snprintf(log, sizeof(log), "Failed to send frame: %d", SSL_get_error(ssl, written));
        log_message(log);
    } else {
        snprintf(log, sizeof(log), "Sent: %s", payload);
        log_message(log);
    }
}

int connect_websocket(SSL** ssl, SSL_CTX* ctx, int* sock, char* session_id, int sequence) {
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        log_message("Socket creation failed");
        return 0;
    }
    int flags = fcntl(*sock, F_GETFL, 0);
    fcntl(*sock, F_SETFL, flags | O_NONBLOCK);

    struct hostent* he = gethostbyname(HOST);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(*sock, (struct sockaddr*)&addr, sizeof(addr)) < 0 && errno != EINPROGRESS) {
        log_message("Connection failed");
        close(*sock);
        return 0;
    }

    fd_set write_fds;
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    FD_ZERO(&write_fds);
    FD_SET(*sock, &write_fds);
    if (select(*sock + 1, NULL, &write_fds, NULL, &tv) <= 0) {
        log_message("Connection timed out");
        close(*sock);
        return 0;
    }

    fcntl(*sock, F_SETFL, flags & ~O_NONBLOCK);

    *ssl = SSL_new(ctx);
    SSL_set_fd(*ssl, *sock);
    SSL_set_tlsext_host_name(*ssl, HOST);
    if (SSL_connect(*ssl) <= 0) {
        log_message("SSL handshake failed");
        close(*sock);
        return 0;
    }

    SSL_set_mode(*ssl, SSL_MODE_AUTO_RETRY);
    flags = fcntl(*sock, F_GETFL, 0);
    fcntl(*sock, F_SETFL, flags | O_NONBLOCK);

    unsigned char key[16];
    RAND_bytes(key, 16);
    char ws_key[25];
    base64_encode(ws_key, key, 16);

    char handshake[512];
    snprintf(handshake, sizeof(handshake),
             "GET /?v=10&encoding=json HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "User-Agent: DiscordBot/1.0\r\n\r\n",
             HOST, ws_key);
    SSL_write(*ssl, handshake, strlen(handshake));

    fcntl(*sock, F_SETFL, flags & ~O_NONBLOCK);
    char buffer[BUFFER_SIZE];
    int bytes = SSL_read(*ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        log_message("Initial read failed");
        close(*sock);
        return 0;
    }
    buffer[bytes] = '\0';
    if (!strstr(buffer, "101 Switching Protocols")) {
        log_message("WebSocket handshake failed");
        close(*sock);
        return 0;
    }
    log_message("WebSocket handshake successful");

    fcntl(*sock, F_SETFL, flags | O_NONBLOCK);

    char identify[512];
    if (session_id[0] != '\0' && sequence > 0) {
        snprintf(identify, sizeof(identify),
                 "{\"op\":6,\"d\":{\"token\":\"%s\",\"session_id\":\"%s\",\"seq\":%d}}",
                 TOKEN, session_id, sequence);
    } else {
        snprintf(identify, sizeof(identify),
                 "{\"op\":2,\"d\":{\"token\":\"%s\",\"intents\":33281,\"properties\":{\"os\":\"linux\",\"browser\":\"custom\",\"device\":\"custom\"}}}",
                 TOKEN);
    }
    send_websocket_frame(*ssl, identify);
    log_message("Sent identify/resume payload");
    return 1;
}

int main() {
    if (!read_keys()) {
        log_message("Failed to initialize, exiting...");
        return 1;
    }
    char log[256];
    snprintf(log, sizeof(log), "Loaded TOKEN: %s, CHANNEL_ID: %s", TOKEN, FIXED_CHANNEL_ID);
    log_message(log);

    log_message("Starting receive.+x");
    init_openssl();
    SSL_CTX* ctx = create_context();
    SSL* ssl = NULL;
    int sock = -1;

    int heartbeat_interval = 0;
    int sequence = 0;
    time_t last_heartbeat = 0;
    char session_id[128] = "";
    char full_payload[MAX_PAYLOAD_SIZE] = {0};
    int payload_len = 0;
    unsigned char current_opcode = 0;
    int is_fragmented = 0;

    FILE* event_fp = fopen("events.txt", "a");
    if (!event_fp) {
        log_message("Failed to open events.txt");
        exit(1);
    }
    setbuf(event_fp, NULL);

    char value[128];
    if (read_state("sequence", value, sizeof(value))) {
        sequence = atoi(value);
    }
    if (read_state("session_id", value, sizeof(value))) {
        strncpy(session_id, value, sizeof(session_id) - 1);
    }

    while (1) {
        if (!ssl) {
            if (!connect_websocket(&ssl, ctx, &sock, session_id, sequence)) {
                log_message("Reconnect failed, retrying in 5s");
                sleep(5);
                continue;
            }
        }

        fd_set read_fds;
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        int sel = select(sock + 1, &read_fds, NULL, NULL, &tv);
        if (sel < 0) {
            log_message("Select error");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(sock);
            ssl = NULL;
            continue;
        }

        if (sel > 0 && FD_ISSET(sock, &read_fds)) {
            char buffer[BUFFER_SIZE];
            int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
            if (bytes <= 0) {
                int ssl_err = SSL_get_error(ssl, bytes);
                if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
                    continue;
                }
                char log[256];
                snprintf(log, sizeof(log), "Connection lost: %d", ssl_err);
                log_message(log);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(sock);
                ssl = NULL;
                continue;
            }

            if (bytes < 2) {
                log_message("Incomplete frame header");
                continue;
            }

            unsigned char opcode = buffer[0] & 0x0F;
            int fin = buffer[0] & 0x80;
            unsigned char len = buffer[1] & 0x7F;
            int offset = 2;
            int expected_len = len;
            if (len == 126) {
                if (bytes < 4) {
                    log_message("Incomplete extended frame");
                    continue;
                }
                expected_len = (buffer[2] << 8) | buffer[3];
                offset = 4;
            } else if (len == 127) {
                if (bytes < 10) {
                    log_message("Incomplete large frame");
                    continue;
                }
                expected_len = 0;
                for (int i = 0; i < 8; i++) {
                    expected_len = (expected_len << 8) | (unsigned char)buffer[2 + i];
                }
                offset = 10;
            }

            int total_bytes_read = bytes - offset;
            char* frame_payload = buffer + offset;
            int frame_payload_len = bytes - offset;

            if (opcode != 0x0) {
                current_opcode = opcode;
            }
            if (!fin && !is_fragmented) {
                is_fragmented = 1;
            }

            if (payload_len + frame_payload_len < MAX_PAYLOAD_SIZE) {
                memcpy(full_payload + payload_len, frame_payload, frame_payload_len);
                payload_len += frame_payload_len;
            } else {
                log_message("Payload buffer overflow, resetting");
                payload_len = 0;
                is_fragmented = 0;
                continue;
            }

            while (total_bytes_read < expected_len) {
                bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
                if (bytes <= 0) {
                    int ssl_err = SSL_get_error(ssl, bytes);
                    if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
                        break;
                    }
                    char log[256];
                    snprintf(log, sizeof(log), "Failed to read full frame: %d", ssl_err);
                    log_message(log);
                    payload_len = 0;
                    is_fragmented = 0;
                    break;
                }
                if (payload_len + bytes < MAX_PAYLOAD_SIZE) {
                    memcpy(full_payload + payload_len, buffer, bytes);
                    payload_len += bytes;
                    total_bytes_read += bytes;
                } else {
                    log_message("Payload buffer overflow during read, resetting");
                    payload_len = 0;
                    is_fragmented = 0;
                    break;
                }
            }

            if (payload_len > 0 && fin) {
                full_payload[payload_len] = '\0';

                if (current_opcode == 0x1) {
                    char log[256];
                    snprintf(log, sizeof(log), "Received: %s", full_payload);
                    log_message(log);

                    if (strstr(full_payload, "\"op\":10")) {
                        sscanf(full_payload, "%*[^h]heartbeat_interval\":%d", &heartbeat_interval);
                        char interval[32];
                        snprintf(interval, sizeof(interval), "%d", heartbeat_interval);
                        update_state("heartbeat_interval", interval);
                        char heartbeat[64];
                        snprintf(heartbeat, sizeof(heartbeat), "{\"op\":1,\"d\":null}");
                        send_websocket_frame(ssl, heartbeat);
                        last_heartbeat = time(NULL);
                    } else if (strstr(full_payload, "\"op\":11")) {
                        log_message("Heartbeat ACK received");
                    } else if (strstr(full_payload, "\"t\":\"MESSAGE_CREATE\"")) {
                        char* content_start = strstr(full_payload, "\"content\":\"");
                        char* channel_start = strstr(full_payload, "\"channel_id\":\"");
                        char* author_start = strstr(full_payload, "\"author\":{\"id\":\"");
                        char content[CONTENT_SIZE] = {0};
                        char channel_id[32] = {0};
                        char author_id[32] = {0};
                        int content_len = 0;

                        if (channel_start) {
                            channel_start += 14;
                            char* channel_end = strchr(channel_start, '"');
                            if (channel_end) {
                                int channel_len = channel_end - channel_start;
                                if (channel_len < sizeof(channel_id)) {
                                    strncpy(channel_id, channel_start, channel_len);
                                    channel_id[channel_len] = '\0';
                                }
                            } else {
                                snprintf(log, sizeof(log), "Failed to parse channel_id in: %s", full_payload);
                                log_message(log);
                            }
                        } else {
                            snprintf(log, sizeof(log), "Missing channel_id in: %s", full_payload);
                            log_message(log);
                        }

                        if (content_start) {
                            content_start += 11;
                            char* content_end = strchr(content_start, '"');
                            if (content_end) {
                                content_len = content_end - content_start;
                                if (content_len >= CONTENT_SIZE - 32) {
                                    content_len = CONTENT_SIZE - 32 - 1;
                                    snprintf(log, sizeof(log), "Warning: Message content truncated: %s", full_payload);
                                    log_message(log);
                                }
                            } else {
                                snprintf(log, sizeof(log), "Failed to parse content in: %s", full_payload);
                                log_message(log);
                            }
                        } else {
                            snprintf(log, sizeof(log), "Missing content in: %s", full_payload);
                            log_message(log);
                        }

                        if (author_start) {
                            author_start += 17;
                            char* author_end = strchr(author_start, '"');
                            if (author_end) {
                                int author_len = author_end - author_start;
                                if (author_len < sizeof(author_id)) {
                                    strncpy(author_id, author_start, author_len);
                                    author_id[author_len] = '\0';
                                }
                                if (content_len > 0) {
                                    snprintf(content, sizeof(content), "<@%s> %.*s", author_id, content_len, content_start);
                                } else {
                                    snprintf(content, sizeof(content), "<@%s>", author_id);
                                }
                            } else {
                                snprintf(log, sizeof(log), "Failed to parse author_id in: %s", full_payload);
                                log_message(log);
                            }
                        } else {
                            snprintf(log, sizeof(log), "Missing author_id in: %s", full_payload);
                            log_message(log);
                            if (content_len > 0) {
                                strncpy(content, content_start, content_len);
                                content[content_len] = '\0';
                            }
                        }

                        if (strlen(channel_id) > 0 && (content_len > 0 || strlen(author_id) > 0)) {
                            snprintf(log, sizeof(log), "New message: %s (channel: %s)", content, channel_id);
                            log_message(log);

                            time_t now = time(NULL);
                            char timestamp[32];
                            snprintf(timestamp, sizeof(timestamp), "%ld", now);
                            char hash_input[CONTENT_SIZE + 32];
                            snprintf(hash_input, sizeof(hash_input), "%s%s", content, timestamp);
                            unsigned int hash = simple_hash(hash_input);
                            char hash_str[7];
                            snprintf(hash_str, sizeof(hash_str), "%06u", hash);
                            fprintf(event_fp, "%s|%s|%s|%s\n", channel_id, timestamp, hash_str, content);
                            fflush(event_fp);
                        } else {
                            snprintf(log, sizeof(log), "Skipping message due to invalid fields: %s", full_payload);
                            log_message(log);
                        }
                    } else if (strstr(full_payload, "\"t\":\"READY\"")) {
                        char* session_start = strstr(full_payload, "\"session_id\":\"");
                        if (session_start) {
                            session_start += 14;
                            char* session_end = strchr(session_start, '"');
                            if (session_end) {
                                int session_len = session_end - session_start;
                                strncpy(session_id, session_start, session_len);
                                session_id[session_len] = '\0';
                                update_state("session_id", session_id);
                            }
                        }
                        log_message("Bot is READY");
                    } else if (strstr(full_payload, "\"op\":1")) {
                        char heartbeat[64];
                        snprintf(heartbeat, sizeof(heartbeat), "{\"op\":1,\"d\":%d}", sequence ? sequence : -1);
                        send_websocket_frame(ssl, heartbeat);
                        last_heartbeat = time(NULL);
                        log_message("Heartbeat sent (requested by server)");
                    }

                    if (strstr(full_payload, "\"s\":")) {
                        sscanf(full_payload, "%*[^s]\"s\":%d", &sequence);
                        char seq[32];
                        snprintf(seq, sizeof(seq), "%d", sequence);
                        update_state("sequence", seq);
                    }
                } else if (current_opcode == 0x8) {
                    if (payload_len >= 2) {
                        unsigned short close_code = ((unsigned char)full_payload[0] << 8) | (unsigned char)full_payload[1];
                        char reason[256] = "";
                        if (payload_len > 2) {
                            strncpy(reason, full_payload + 2, payload_len - 2);
                            reason[payload_len - 2] = '\0';
                        }
                        char log[256];
                        snprintf(log, sizeof(log), "Close frame - Code: %d, Reason: %s", close_code, reason);
                        log_message(log);
                        if (close_code == 4000 || close_code == 4007) {
                            session_id[0] = '\0';
                            sequence = 0;
                            update_state("session_id", "");
                            update_state("sequence", "0");
                        }
                    }
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                    close(sock);
                    ssl = NULL;
                    sleep(5);
                    continue;
                }

                payload_len = 0;
                memset(full_payload, 0, MAX_PAYLOAD_SIZE);
                is_fragmented = 0;
                current_opcode = 0;
            }
        }

        time_t now = time(NULL);
        if (heartbeat_interval > 0 && (now - last_heartbeat) * 1000 >= heartbeat_interval) {
            char heartbeat[64];
            snprintf(heartbeat, sizeof(heartbeat), "{\"op\":1,\"d\":%d}", sequence ? sequence : -1);
            send_websocket_frame(ssl, heartbeat);
            last_heartbeat = now;
            log_message("Heartbeat sent");
        }
    }

    fclose(event_fp);
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sock);
    }
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
