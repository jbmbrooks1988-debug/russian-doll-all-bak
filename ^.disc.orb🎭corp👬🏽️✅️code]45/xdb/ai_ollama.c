#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>


#define OLLAMA_RESPONSE_FILE "ollama_response.txt"
#define OLLAMA_HOST "localhost"
#define OLLAMA_PORT 11434
#define OLLAMA_ENDPOINT "/api/generate"
#define LOG_FILE "ai_ollama_log.txt"

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) { 
        flock(fileno(f), LOCK_EX);
        fprintf(f, "[%ld] %s\n", time(NULL), msg); 
        flock(fileno(f), LOCK_UN);
        fclose(f); 
    }
    printf("[%ld] %s\n", time(NULL), msg);
}

// Function to initialize Ollama (placeholder for future setup)
void init_ollama() {
    log_message("Ollama initialized");
}

// Function to process a message and interact with Ollama using raw sockets
void process_ollama_chat(const char* message) {
    char log[256];
    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent* server;
    char request[4096];
    char response[8192] = {0};
    char post_data[2048];

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        snprintf(log, sizeof(log), "Failed to create socket: %s", strerror(errno));
        log_message(log);
        return;
    }

    // Resolve hostname
    server = gethostbyname(OLLAMA_HOST);
    if (!server) {
        snprintf(log, sizeof(log), "Failed to resolve %s", OLLAMA_HOST);
        log_message(log);
        close(sockfd);
        return;
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(OLLAMA_PORT);
    memcpy(&server_addr.sin_addr, server->h_addr_list[0], server->h_length);

    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        snprintf(log, sizeof(log), "Failed to connect to %s:%d: %s", OLLAMA_HOST, OLLAMA_PORT, strerror(errno));
        log_message(log);
        close(sockfd);
        return;
    }

    // Construct JSON payload
    snprintf(post_data, sizeof(post_data),
             "{\"model\": \"llama3\", \"prompt\": \"%s\", \"stream\": false}",
             message);

    // Construct HTTP POST request
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             OLLAMA_ENDPOINT, OLLAMA_HOST, strlen(post_data), post_data);

    // Send request
    if (send(sockfd, request, strlen(request), 0) < 0) {
        snprintf(log, sizeof(log), "Failed to send request: %s", strerror(errno));
        log_message(log);
        close(sockfd);
        return;
    }

    // Receive response
    int bytes_received;
    int total_bytes = 0;
    while ((bytes_received = recv(sockfd, response + total_bytes, sizeof(response) - total_bytes - 1, 0)) > 0) {
        total_bytes += bytes_received;
        response[total_bytes] = '\0';
    }
    if (bytes_received < 0) {
        snprintf(log, sizeof(log), "Failed to receive response: %s", strerror(errno));
        log_message(log);
        close(sockfd);
        return;
    }

    // Close socket
    close(sockfd);

    // Find the start of the JSON body (skip HTTP headers)
    char* body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4; // Skip past "\r\n\r\n"
    } else {
        body = response; // Fallback to full response if no headers
    }

    snprintf(log, sizeof(log), "Received response: %.100s...", body);
    log_message(log);

    // Write response to file
    FILE* fp = fopen(OLLAMA_RESPONSE_FILE, "w");
    if (fp) {
        flock(fileno(fp), LOCK_EX);
        fprintf(fp, "%s", body);
        flock(fileno(fp), LOCK_UN);
        fclose(fp);
        log_message("Wrote response to ollama_response.txt");
    } else {
        snprintf(log, sizeof(log), "Failed to open %s: %s", OLLAMA_RESPONSE_FILE, strerror(errno));
        log_message(log);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log_message("Usage: ai_ollama.+x <message>");
        return 1;
    }

    const char* message = argv[1];
    init_ollama();
    process_ollama_chat(message);

    return 0;
}
