#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <fcntl.h>

// Log to server.log
void log_to_file(const char* message) {
    FILE* fp = fopen("server.log", "a");
    if (fp) {
        time_t now = time(NULL);
        char* timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = '\0';
        fprintf(fp, "[%s] %s\n", timestamp, message);
        fclose(fp);
    }
}

// Get MIME type based on file extension
const char* get_mime_type(const char* path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".txt")) return "text/plain";
    return "application/octet-stream";
}

// Serve a file
void serve_file(int client_sock, const char* path) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Requested file: %s", path);
    log_to_file(log_msg);

    // Default to index.html if path is "/"
 //    const char* filepath = (strcmp(path, "/") == 0) ? "index.html" : path + 1;
    const char* filepath = (strcmp(path, "/") == 0) ? "3dwebgl♟♟️]q23]b1.html" : path + 1;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        snprintf(log_msg, sizeof(log_msg), "Error: File %s not found", filepath);
        log_to_file(log_msg);
        const char* response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\nFile Not Found";
        write(client_sock, response, strlen(response));
        return;
    }

    // Get file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // Read file content
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        log_to_file("Error: Memory allocation failed");
        close(fd);
        const char* response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\n\r\nMemory Allocation Failed";
        write(client_sock, response, strlen(response));
        return;
    }
    ssize_t bytes_read = read(fd, buffer, file_size);
    buffer[bytes_read] = '\0';
    close(fd);

    // Send HTTP response
    char response[1024];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
             get_mime_type(filepath), file_size);
    write(client_sock, response, strlen(response));
    write(client_sock, buffer, file_size);
    snprintf(log_msg, sizeof(log_msg), "Served file %s (%ld bytes)", filepath, file_size);
    log_to_file(log_msg);
    free(buffer);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        log_to_file("Error: Failed to create socket");
        return 1;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_to_file("Error: Failed to bind socket");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        log_to_file("Error: Failed to listen on socket");
        close(server_sock);
        return 1;
    }

    log_to_file("Server started on http://localhost:8080");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            log_to_file("Error: Failed to accept connection");
            continue;
        }

        char buffer[1024];
        ssize_t bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
        if (bytes_read < 0) {
            log_to_file("Error: Failed to read request");
            close(client_sock);
            continue;
        }
        buffer[bytes_read] = '\0';

        // Parse GET request
        char* method = strtok(buffer, " ");
        char* path = strtok(NULL, " ");
        if (method && path && strcmp(method, "GET") == 0) {
            serve_file(client_sock, path);
        } else {
            log_to_file("Error: Invalid or non-GET request");
            const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            write(client_sock, response, strlen(response));
        }

        close(client_sock);
    }

    close(server_sock);
    return 0;
}
