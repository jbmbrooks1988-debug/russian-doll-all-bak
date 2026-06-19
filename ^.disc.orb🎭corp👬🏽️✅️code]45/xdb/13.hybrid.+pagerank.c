#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_URL 512
#define MAX_HTML 1048576 // 1MB for HTML response
#define MAX_TEXT 524288 // 512KB for extracted text
#define MAX_PATH 256
#define HTTP_PORT 80
#define HTTPS_PORT 443
#define MAX_DOMAINS 15 // Limit for POC
#define TOP_N 6 // Top 6 results (3 domains + 3 pages)
#define MAX_DOMAIN_HITS 3 // 3 domain results
#define MAX_PAGE_HITS 1 // 1 linked page per domain
#define MAX_LINKS 50 // Max links per page
#define CONTEXT_WORDS 20 // Number of words for context
#define CONTEXT_BUFFER 2048 // Buffer for context string

// Convert string to lowercase
void to_lowercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 32;
        }
    }
}

// Replace spaces with underscores
void sanitize_keyword(const char *keyword, char *safe_keyword, size_t max_len) {
    if (!keyword || !safe_keyword) return;
    strncpy(safe_keyword, keyword, max_len - 1);
    safe_keyword[max_len - 1] = '\0';
    for (int i = 0; safe_keyword[i]; i++) {
        if (safe_keyword[i] == ' ') {
            safe_keyword[i] = '_';
        }
    }
}

// Extract domain from URL
char *extract_domain(const char *url) {
    if (!url) return NULL;
    char *start = strstr(url, "://");
    if (!start) start = (char *)url;
    else start += 3;
    char *end = strchr(start, '/');
    if (!end) end = start + strlen(start);
    int len = end - start;
    if (len <= 0 || len > MAX_URL - 1) return NULL;
    char *domain = malloc(len + 1);
    if (!domain) return NULL;
    strncpy(domain, start, len);
    domain[len] = '\0';
    char *dot = strchr(domain, ':');
    if (dot) *dot = '\0'; // Remove port
    return domain;
}

// Extract path from URL
char *extract_path(const char *url) {
    if (!url) return NULL;
    char *start = strstr(url, "://");
    if (!start) return strdup("/");
    start = strchr(start + 3, '/');
    if (!start) return strdup("/");
    char *path = strdup(start);
    if (!path) return NULL;
    char *query = strchr(path, '?');
    if (query) *query = '\0'; // Truncate query string
    return path;
}

// Debug logging for links
void log_pagerank_debug(const char *message) {
    FILE *fp = fopen("pagerank_debug.txt", "a");
    if (!fp) {
        fprintf(stderr, "Failed to open pagerank_debug.txt: %s\n", strerror(errno));
        return;
    }
    fprintf(fp, "%s\n", message);
    fclose(fp);
}

// Send HTTP/HTTPS GET request
int send_http_get(const char *host, const char *path, char *response, size_t max_size, int use_ssl, char *redirect_url, size_t redirect_size) {
    if (!host || !path || !response || !redirect_url) {
        fprintf(stderr, "Invalid input parameters\n");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(use_ssl ? HTTPS_PORT : HTTP_PORT);

    struct hostent *server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "Host resolution failed for %s\n", host);
        close(sock);
        return -1;
    }
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection failed to %s:%d: %s\n", host, use_ssl ? HTTPS_PORT : HTTP_PORT, strerror(errno));
        close(sock);
        return -1;
    }

    SSL *ssl = NULL;
    SSL_CTX *ctx = NULL;
    if (use_ssl) {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            fprintf(stderr, "SSL_CTX_new failed\n");
            close(sock);
            return -1;
        }
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
        const char *ciphers = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";
        if (SSL_CTX_set_cipher_list(ctx, ciphers) != 1) {
            fprintf(stderr, "Failed to set cipher list\n");
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
        ssl = SSL_new(ctx);
        if (!ssl) {
            fprintf(stderr, "SSL_new failed\n");
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
        if (SSL_set_fd(ssl, sock) == 0) {
            fprintf(stderr, "SSL_set_fd failed\n");
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
        if (SSL_connect(ssl) != 1) {
            fprintf(stderr, "SSL_connect failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
    }

    char request[MAX_URL * 2];
    int req_len = snprintf(request, sizeof(request),
                           "GET %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36\r\n"
                           "Accept: text/html,application/xhtml+xml\r\n"
                           "Accept-Language: en-US,en;q=0.5\r\n"
                           "Accept-Encoding: identity\r\n"
                           "Connection: close\r\n\r\n",
                           path, host);
    if (req_len >= sizeof(request)) {
        fprintf(stderr, "Request buffer overflow for %s\n", host);
        if (use_ssl) {
            SSL_free(ssl);
            SSL_CTX_free(ctx);
        }
        close(sock);
        return -1;
    }

    if (use_ssl) {
        if (SSL_write(ssl, request, strlen(request)) <= 0) {
            fprintf(stderr, "SSL_write failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
    } else {
        if (send(sock, request, strlen(request), 0) < 0) {
            fprintf(stderr, "Send failed: %s\n", strerror(errno));
            close(sock);
            return -1;
        }
    }

    size_t total = 0;
    response[0] = '\0';
    char buffer[4096];
    while (total < max_size - 1) {
        ssize_t received;
        if (use_ssl) {
            received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        } else {
            received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        }
        if (received <= 0) break;
        buffer[received] = '\0';
        if (total + received < max_size) {
            memcpy(response + total, buffer, received);
            total += received;
            response[total] = '\0';
        } else {
            fprintf(stderr, "Response too large for buffer from %s\n", host);
            if (use_ssl) {
                SSL_free(ssl);
                SSL_CTX_free(ctx);
            }
            close(sock);
            return -1;
        }
    }

    if (strstr(response, "HTTP/1.1 301") || strstr(response, "HTTP/1.1 302")) {
        char *location = strstr(response, "Location: ");
        if (location) {
            location += 10;
            char *end = strstr(location, "\r\n");
            if (end) {
                int len = end - location;
                if (len > 0 && len < redirect_size) {
                    strncpy(redirect_url, location, len);
                    redirect_url[len] = '\0';
                    total = -2;
                }
            }
        }
    }

    if (use_ssl) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    }
    close(sock);
    return total;
}

// Count keyword occurrences in HTML
int count_keyword(const char *html, const char *keyword) {
    if (!html || !keyword) {
        fprintf(stderr, "Invalid html or keyword\n");
        return 0;
    }
    int count = 0;
    char lower_keyword[MAX_URL];
    strncpy(lower_keyword, keyword, MAX_URL - 1);
    lower_keyword[MAX_URL - 1] = '\0';
    to_lowercase(lower_keyword);

    char *lower_html = strdup(html);
    if (!lower_html) {
        fprintf(stderr, "Failed to allocate lower_html\n");
        return 0;
    }
    to_lowercase(lower_html);

    char *ptr = lower_html;
    while ((ptr = strstr(ptr, lower_keyword))) {
        count++;
        ptr += strlen(lower_keyword);
    }
    free(lower_html);
    return count;
}

// Extract text from HTML
long extract_text(const char *html, char *text, size_t max_size) {
    if (!html || !text) {
        fprintf(stderr, "Invalid html or text buffer\n");
        return -1;
    }
    char *body = strstr(html, "<body");
    if (!body) body = strstr(html, "<BODY");
    if (!body) body = html;
    else {
        body = strchr(body, '>');
        if (body) body++;
    }
    if (!body) body = html;

    size_t text_len = 0;
    int in_tag = 0, in_script = 0;
    for (char *p = body; *p && text_len < max_size - 1; p++) {
        if (*p == '<') {
            in_tag = 1;
            if (strncmp(p, "<script", 7) == 0 || strncmp(p, "<SCRIPT", 7) == 0) {
                in_script = 1;
            }
        } else if (*p == '>') {
            in_tag = 0;
            if (in_script && (strncmp(p - 8, "</script>", 9) == 0 || strncmp(p - 8, "</SCRIPT>", 9) == 0)) {
                in_script = 0;
            }
        } else if (!in_tag && !in_script) {
            if ((*p >= 32 && *p <= 126) || *p == '\n' || *p == '\t' || *p == '.' || *p == ',' || *p == '!' || *p == '?') {
                text[text_len++] = *p;
            }
        }
    }
    text[text_len] = '\0';
    return text_len;
}

// Extract keyword context (20 words around each occurrence)
long extract_keyword_context(const char *text, const char *keyword, char *context_output, size_t max_size) {
    if (!text || !keyword || !context_output) {
        fprintf(stderr, "Invalid input for extract_keyword_context\n");
        return -1;
    }

    char *lower_text = strdup(text);
    if (!lower_text) {
        fprintf(stderr, "Failed to allocate lower_text\n");
        return -1;
    }
    to_lowercase(lower_text);

    char lower_keyword[MAX_URL];
    strncpy(lower_keyword, keyword, MAX_URL - 1);
    lower_keyword[MAX_URL - 1] = '\0';
    to_lowercase(lower_keyword);

    size_t context_len = 0;
    context_output[0] = '\0';
    char *ptr = lower_text;
    int instance_count = 0;

    while ((ptr = strstr(ptr, lower_keyword)) && context_len < max_size - 1) {
        instance_count++;
        char instance_context[CONTEXT_BUFFER] = {0};
        size_t keyword_len = strlen(lower_keyword);
        char *start = ptr;
        int words_before = 0, words_after = 0;

        // Move backward to find start of context
        while (start > lower_text && words_before < CONTEXT_WORDS / 2) {
            if (start[-1] == ' ' || start[-1] == '\n' || start[-1] == '\t') {
                words_before++;
            }
            start--;
        }
        if (start > lower_text && (start[-1] == ' ' || start[-1] == '\n' || start[-1] == '\t')) {
            start++;
        }

        // Move forward to find end of context
        char *end = ptr + keyword_len;
        while (*end && words_after < CONTEXT_WORDS / 2) {
            if (*end == ' ' || *end == '\n' || *end == '\t') {
                words_after++;
            }
            end++;
        }

        // Copy context from original text (not lowercased)
        size_t orig_start_offset = ptr - lower_text;
        char *orig_start = (char *)text + orig_start_offset - (ptr - start);
        size_t context_size = end - start;
        if (context_size >= CONTEXT_BUFFER) {
            context_size = CONTEXT_BUFFER - 1;
        }

        strncpy(instance_context, orig_start, context_size);
        instance_context[context_size] = '\0';

        // Remove newlines and tabs
        for (int i = 0; instance_context[i]; i++) {
            if (instance_context[i] == '\n' || instance_context[i] == '\t') {
                instance_context[i] = ' ';
            }
        }

        // Append to context_output
        size_t instance_len = strlen(instance_context);
        if (context_len + instance_len + 50 < max_size) {
            char header[50];
            snprintf(header, sizeof(header), "Instance %d: ", instance_count);
            strncat(context_output, header, max_size - context_len - 1);
            context_len += strlen(header);
            strncat(context_output, instance_context, max_size - context_len - 1);
            context_len += instance_len;
            strncat(context_output, "\n", max_size - context_len - 1);
            context_len += 1;
        }

        ptr += keyword_len;
    }

    free(lower_text);
    return context_len;
}

// Extract links from HTML
int extract_links(const char *html, char **links, int max_links) {
    if (!html || !links) return 0;
    int count = 0;
    char *ptr = (char *)html;
    char debug_msg[1024];
    snprintf(debug_msg, sizeof(debug_msg), "Extracting links for HTML content (length: %zu)", strlen(html));
    log_pagerank_debug(debug_msg);

    while (count < max_links && (ptr = strstr(ptr, "<a "))) {
        ptr = strstr(ptr, "href=");
        if (!ptr) break;
        ptr += 5;
        char quote = *ptr;
        if (quote != '"' && quote != '\'') {
            ptr++;
            continue;
        }
        ptr++;
        char *end = strchr(ptr, quote);
        if (!end) break;
        int len = end - ptr;
        if (len <= 0 || len >= MAX_URL) {
            ptr = end;
            continue;
        }
        char *url = malloc(len + 1);
        if (!url) {
            ptr = end;
            continue;
        }
        strncpy(url, ptr, len);
        url[len] = '\0';
        if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
            snprintf(debug_msg, sizeof(debug_msg), "Skipping non-HTTP link: %s", url);
            log_pagerank_debug(debug_msg);
            free(url);
            ptr = end;
            continue;
        }
        links[count] = url; // Store full URL for linked pages
        snprintf(debug_msg, sizeof(debug_msg), "Found link: %s", url);
        log_pagerank_debug(debug_msg);
        count++;
        ptr = end;
    }
    snprintf(debug_msg, sizeof(debug_msg), "Total links extracted: %d", count);
    log_pagerank_debug(debug_msg);
    return count;
}

// Save content to file
long save_content(const char *text, const char *path) {
    if (!text || !path || strlen(text) < 5) {
        fprintf(stderr, "Insufficient text extracted (%zu bytes)\n", text ? strlen(text) : 0);
        return -1;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s: %s\n", path, strerror(errno));
        return -1;
    }
    size_t len = strlen(text);
    fwrite(text, 1, len, fp);
    fclose(fp);
    return len;
}

// Log result to dns_results.txt
void log_result(const char *identifier, const char *keyword, int count, const char *text, int source) {
    if (!identifier || !keyword || !text) {
        fprintf(stderr, "Invalid log parameters\n");
        return;
    }
    FILE *fp = fopen("dns_results.txt", "a");
    if (!fp) {
        fprintf(stderr, "Failed to open dns_results.txt: %s\n", strerror(errno));
        return;
    }
    const char *source_str = source == 0 ? "dns" : (source == 2 ? "page" : "unknown");
    fprintf(fp, "%s: %s, Keyword: %s, Mentions: %d, Source: %s, Text: %.1000s\n",
            source == 2 ? "Page" : "Domain", identifier, keyword, count, source_str, text);
    fclose(fp);
}

// Count inbound links and store source domains
void count_inbound_links(char **pages, int page_count, char **links, int *link_counts, int *inbound_counts, char ***inbound_sources, int max_links) {
    if (!pages || !links || !inbound_counts || !inbound_sources || page_count <= 0) return;
    char debug_msg[1024];
    snprintf(debug_msg, sizeof(debug_msg), "Counting inbound links for %d pages", page_count);
    log_pagerank_debug(debug_msg);

    for (int i = 0; i < page_count; i++) {
        inbound_counts[i] = 0;
        inbound_sources[i] = calloc(max_links, sizeof(char *));
        if (!inbound_sources[i]) {
            snprintf(debug_msg, sizeof(debug_msg), "Failed to allocate inbound_sources[%d]", i);
            log_pagerank_debug(debug_msg);
        }
    }

    for (int i = 0; i < page_count; i++) {
        for (int j = 0; j < link_counts[i] && j < max_links; j++) {
            char *link = links[i * max_links + j];
            if (!link) continue;
            char *link_domain = extract_domain(link);
            if (!link_domain) continue;
            for (int k = 0; k < page_count; k++) {
                if (pages[k] && strcmp(pages[k], link_domain) == 0) {
                    if (inbound_counts[k] < max_links && inbound_sources[k]) {
                        inbound_sources[k][inbound_counts[k]] = strdup(pages[i]);
                        if (inbound_sources[k][inbound_counts[k]]) {
                            inbound_counts[k]++;
                            snprintf(debug_msg, sizeof(debug_msg), "Inbound link to %s from %s", pages[k], pages[i]);
                            log_pagerank_debug(debug_msg);
                        }
                    }
                    break;
                }
            }
            free(link_domain);
        }
    }
}

// Read domains from dns_domains.txt
char **read_dns_domains(int *count) {
    FILE *fp = fopen("dns_domains.txt", "r");
    if (!fp) {
        fprintf(stderr, "Failed to open dns_domains.txt: %s\n", strerror(errno));
        *count = 0;
        return NULL;
    }

    char **domains = calloc(MAX_DOMAINS + 1, sizeof(char *));
    if (!domains) {
        fprintf(stderr, "Failed to allocate memory for domains\n");
        fclose(fp);
        *count = 0;
        return NULL;
    }

    char line[MAX_URL];
    int i = 0;
    while (i < MAX_DOMAINS && fgets(line, MAX_URL, fp)) {
        // Remove trailing newline and whitespace
        line[strcspn(line, "\n\r")] = '\0';
        if (strlen(line) == 0) continue; // Skip empty lines
        domains[i] = strdup(line);
        if (!domains[i]) {
            fprintf(stderr, "Failed to allocate memory for domain %s\n", line);
            continue;
        }
        i++;
    }
    domains[i] = NULL; // Null-terminate the array
    *count = i;
    fclose(fp);
    return domains;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s \"keyword\" <txt|html>\n", argv[0]);
        return 1;
    }

    char file_type[MAX_PATH];
    strncpy(file_type, argv[2], MAX_PATH - 1);
    file_type[MAX_PATH - 1] = '\0';
    to_lowercase(file_type);
    if (strcmp(file_type, "txt") != 0 && strcmp(file_type, "html") != 0) {
        fprintf(stderr, "File type must be 'txt' or 'html'\n");
        return 1;
    }
    int save_html = (strcmp(file_type, "html") == 0);

    char keyword[MAX_URL];
    char safe_keyword[MAX_URL];
    strncpy(keyword, argv[1], MAX_URL - 1);
    keyword[MAX_URL - 1] = '\0';
    sanitize_keyword(keyword, safe_keyword, MAX_URL);

    char dir_path[MAX_PATH];
    char sub_dir_path[MAX_PATH];
    snprintf(dir_path, MAX_PATH, "text-dl");
    snprintf(sub_dir_path, MAX_PATH, "text-dl/%.200s", safe_keyword); // Limit safe_keyword to avoid overflow
    if (mkdir(dir_path, 0755) && errno != EEXIST) {
        perror("Failed to create text-dl directory");
        return 1;
    }
    if (mkdir(sub_dir_path, 0755) && errno != EEXIST) {
        perror("Failed to create subdirectory");
        return 1;
    }

    // Clear pagerank_debug.txt and add keyword
    FILE *fp = fopen("pagerank_debug.txt", "w");
    if (fp) fclose(fp);
    fprintf(fopen("pagerank_debug.txt", "a"), "Keyword: %s\n", keyword);

    // Read domains from dns_domains.txt
    int domain_list_count = 0;
    char **dns_domains = read_dns_domains(&domain_list_count);
    if (!dns_domains || domain_list_count == 0) {
        fprintf(stderr, "No domains loaded from dns_domains.txt\n");
        if (dns_domains) free(dns_domains);
        return 1;
    }

    // Arrays for results
    char *domains_list[MAX_DOMAIN_HITS] = {0};
    char *domains_texts[MAX_DOMAIN_HITS] = {0};
    int domains_counts[MAX_DOMAIN_HITS] = {0};
    int domains_inbound[MAX_DOMAIN_HITS] = {0};
    char **domains_inbound_sources[MAX_DOMAIN_HITS] = {0};
    int domain_count = 0;

    char *pages_list[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char *pages_texts[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int pages_counts[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int pages_inbound[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char **pages_inbound_sources[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int page_count = 0;

    // Link data
    char *all_pages[MAX_DOMAINS + MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char *all_links[(MAX_DOMAINS + MAX_DOMAINS * MAX_PAGE_HITS) * MAX_LINKS] = {0};
    int link_counts[MAX_DOMAINS + MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int inbound_counts[MAX_DOMAINS + MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char **inbound_sources[MAX_DOMAINS + MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int all_pages_count = 0;

    // Query domains
    for (int i = 0; i < domain_list_count && domain_count < MAX_DOMAIN_HITS && all_pages_count < MAX_DOMAINS; i++) {
        if (!dns_domains[i]) continue;
        char html[MAX_HTML] = {0};
        char redirect_url[MAX_URL] = {0};
        int html_size = send_http_get(dns_domains[i], "/", html, MAX_HTML, 1, redirect_url, MAX_URL);
        int use_ssl = 1;

        if (html_size == -2 && redirect_url[0]) {
            fprintf(stderr, "Redirect detected to %s\n", redirect_url);
            char new_host[MAX_URL] = "", new_path[MAX_URL] = "/";
            if (strncmp(redirect_url, "https://", 8) == 0) {
                sscanf(redirect_url, "https://%[^/]/%[^\n]", new_host, new_path);
            } else if (strncmp(redirect_url, "http://", 7) == 0) {
                use_ssl = 0;
                sscanf(redirect_url, "http://%[^/]/%[^\n]", new_host, new_path);
            } else {
                fprintf(stderr, "Invalid redirect URL: %s\n", redirect_url);
                continue;
            }
            html_size = send_http_get(new_host, new_path[0] ? new_path : "/", html, MAX_HTML, use_ssl, redirect_url, MAX_URL);
        }

        if (html_size <= 0) {
            fprintf(stderr, "Retrying %s with HTTP\n", dns_domains[i]);
            html_size = send_http_get(dns_domains[i], "/", html, MAX_HTML, 0, redirect_url, MAX_URL);
            if (html_size <= 0) {
                fprintf(stderr, "Failed to fetch %s\n", dns_domains[i]);
                continue;
            }
        }
        printf("Fetched %d bytes from %s\n", html_size, dns_domains[i]);

        int count = count_keyword(html, keyword);
        char *text = malloc(MAX_TEXT);
        if (!text) {
            fprintf(stderr, "Failed to allocate text buffer for %s\n", dns_domains[i]);
            continue;
        }
        text[0] = '\0';
        long text_len = save_html ? html_size : extract_text(html, text, MAX_TEXT);
        if (text_len < (save_html ? 1 : 5)) {
            fprintf(stderr, "Insufficient content extracted from %s (%ld bytes)\n", dns_domains[i], text_len);
            free(text);
            continue;
        }

        // Add to link graph
        all_pages[all_pages_count] = strdup(dns_domains[i]);
        if (!all_pages[all_pages_count]) {
            free(text);
            continue;
        }
        char *links[MAX_LINKS] = {0};
        int num_links = extract_links(html, links, MAX_LINKS);
        for (int j = 0; j < num_links && j < MAX_LINKS; j++) {
            if (links[j]) {
                all_links[all_pages_count * MAX_LINKS + link_counts[all_pages_count]] = links[j];
                link_counts[all_pages_count]++;
                links[j] = NULL; // Prevent double-free
            }
        }

        if (count > 0) {
            log_result(dns_domains[i], keyword, count, save_html ? html : text, 0);
            domains_list[domain_count] = strdup(dns_domains[i]);
            if (!domains_list[domain_count]) {
                free(text);
                continue;
            }
            domains_texts[domain_count] = text;
            domains_counts[domain_count] = count;
            domains_inbound_sources[domain_count] = calloc(MAX_LINKS, sizeof(char *));
            if (!domains_inbound_sources[domain_count]) {
                free(text);
                free(domains_list[domain_count]);
                continue;
            }
            domain_count++;
            printf("Found domain: %s (%d keyword matches)\n", domains_list[domain_count - 1], count);
        } else {
            fprintf(stderr, "No keyword matches for %s\n", dns_domains[i]);
            free(text);
        }

        // Crawl linked pages (1 per domain)
        for (int j = 0; j < num_links && j < MAX_PAGE_HITS && page_count < MAX_DOMAINS * MAX_PAGE_HITS; j++) {
            if (!all_links[all_pages_count * MAX_LINKS + j]) continue;
            char *link = all_links[all_pages_count * MAX_LINKS + j];
            char *link_domain = extract_domain(link);
            if (!link_domain) continue;
            char *link_path = extract_path(link);
            if (!link_path) {
                free(link_domain);
                continue;
            }

            char page_html[MAX_HTML] = {0};
            char page_redirect_url[MAX_URL] = {0};
            int page_html_size = send_http_get(link_domain, link_path, page_html, MAX_HTML, 1, page_redirect_url, MAX_URL);
            use_ssl = 1;

            if (page_html_size == -2 && page_redirect_url[0]) {
                fprintf(stderr, "Redirect detected to %s for linked page\n", page_redirect_url);
                char new_host[MAX_URL] = "", new_path[MAX_URL] = "/";
                if (strncmp(page_redirect_url, "https://", 8) == 0) {
                    sscanf(page_redirect_url, "https://%[^/]/%[^\n]", new_host, new_path);
                } else if (strncmp(page_redirect_url, "http://", 7) == 0) {
                    use_ssl = 0;
                    sscanf(page_redirect_url, "http://%[^/]/%[^\n]", new_host, new_path);
                } else {
                    fprintf(stderr, "Invalid redirect URL: %s\n", page_redirect_url);
                    free(link_domain);
                    free(link_path);
                    continue;
                }
                page_html_size = send_http_get(new_host, new_path[0] ? new_path : "/", page_html, MAX_HTML, use_ssl, page_redirect_url, MAX_URL);
            }

            if (page_html_size <= 0) {
                fprintf(stderr, "Retrying linked page %s with HTTP\n", link);
                page_html_size = send_http_get(link_domain, link_path, page_html, MAX_HTML, 0, page_redirect_url, MAX_URL);
                if (page_html_size <= 0) {
                    fprintf(stderr, "Failed to fetch linked page %s\n", link);
                    free(link_domain);
                    free(link_path);
                    continue;
                }
            }
            printf("Fetched %d bytes from linked page %s\n", page_html_size, link);

            int page_count_keywords = count_keyword(page_html, keyword);
            char *page_text = malloc(MAX_TEXT);
            if (!page_text) {
                fprintf(stderr, "Failed to allocate text buffer for linked page %s\n", link);
                free(link_domain);
                free(link_path);
                continue;
            }
            page_text[0] = '\0';
            long page_text_len = save_html ? page_html_size : extract_text(page_html, page_text, MAX_TEXT);
            if (page_text_len < (save_html ? 1 : 5)) {
                fprintf(stderr, "Insufficient content extracted from linked page %s (%ld bytes)\n", link, page_text_len);
                free(page_text);
                free(link_domain);
                free(link_path);
                continue;
            }

            if (page_count_keywords > 0) {
                log_result(link, keyword, page_count_keywords, save_html ? page_html : page_text, 2);
                pages_list[page_count] = strdup(link);
                if (!pages_list[page_count]) {
                    free(page_text);
                    free(link_domain);
                    free(link_path);
                    continue;
                }
                pages_texts[page_count] = page_text;
                pages_counts[page_count] = page_count_keywords;
                pages_inbound_sources[page_count] = calloc(MAX_LINKS, sizeof(char *));
                if (!pages_inbound_sources[page_count]) {
                    free(page_text);
                    free(pages_list[page_count]);
                    free(link_domain);
                    free(link_path);
                    continue;
                }
                page_count++;
                printf("Found linked page: %s (%d keyword matches)\n", pages_list[page_count - 1], page_count_keywords);
            } else {
                fprintf(stderr, "No keyword matches for linked page %s\n", link);
                free(page_text);
            }
            free(link_domain);
            free(link_path);
        }
        all_pages_count++;
    }

    // Count inbound links and store sources
    if (all_pages_count > 0) {
        count_inbound_links(all_pages, all_pages_count, all_links, link_counts, inbound_counts, inbound_sources, MAX_LINKS);
    }

    // Assign inbound link counts and sources to domains
    for (int i = 0; i < domain_count; i++) {
        for (int j = 0; j < all_pages_count; j++) {
            if (all_pages[j] && strcmp(all_pages[j], domains_list[i]) == 0) {
                domains_inbound[i] = inbound_counts[j];
                for (int k = 0; k < inbound_counts[j] && k < MAX_LINKS; k++) {
                    if (inbound_sources[j][k]) {
                        domains_inbound_sources[i][k] = strdup(inbound_sources[j][k]);
                    }
                }
                break;
            }
        }
    }

    // Assign inbound link counts and sources to pages
    for (int i = 0; i < page_count; i++) {
        char *page_domain = extract_domain(pages_list[i]);
        if (!page_domain) continue;
        for (int j = 0; j < all_pages_count; j++) {
            if (all_pages[j] && strcmp(all_pages[j], page_domain) == 0) {
                pages_inbound[i] = inbound_counts[j];
                for (int k = 0; k < inbound_counts[j] && k < MAX_LINKS; k++) {
                    if (inbound_sources[j][k]) {
                        pages_inbound_sources[i][k] = strdup(inbound_sources[j][k]);
                    }
                }
                break;
            }
        }
        free(page_domain);
    }

    printf("Total domains found: %d\n", domain_count);
    printf("Total linked pages found: %d\n", page_count);

    // Merge results
    struct result {
        char *identifier;
        char *text;
        int count;
        int inbound;
        char **inbound_sources;
        int source; // 0=dns, 2=page
        char *context; // Store context for display and save
    } results[TOP_N] = {0};
    int total_results = 0;

    // Add domains (up to 3)
    for (int i = 0; i < domain_count && total_results < TOP_N && i < MAX_DOMAIN_HITS; i++) {
        results[total_results].identifier = domains_list[i];
        results[total_results].text = domains_texts[i];
        results[total_results].count = domains_counts[i];
        results[total_results].inbound = domains_inbound[i];
        results[total_results].inbound_sources = domains_inbound_sources[i];
        results[total_results].source = 0;
        results[total_results].context = malloc(MAX_TEXT);
        if (results[total_results].context && !save_html) {
            results[total_results].context[0] = '\0';
            extract_keyword_context(domains_texts[i], keyword, results[total_results].context, MAX_TEXT);
        }
        total_results++;
    }

    // Add linked pages (up to TOP_N - domains)
    for (int i = 0; i < page_count && total_results < TOP_N && i < MAX_DOMAINS * MAX_PAGE_HITS; i++) {
        results[total_results].identifier = pages_list[i];
        results[total_results].text = pages_texts[i];
        results[total_results].count = pages_counts[i];
        results[total_results].inbound = pages_inbound[i];
        results[total_results].inbound_sources = pages_inbound_sources[i];
        results[total_results].source = 2;
        results[total_results].context = malloc(MAX_TEXT);
        if (results[total_results].context && !save_html) {
            results[total_results].context[0] = '\0';
            extract_keyword_context(pages_texts[i], keyword, results[total_results].context, MAX_TEXT);
        }
        total_results++;
    }

    if (total_results == 0) {
        fprintf(stderr, "No domains or pages with keyword matches\n");
        goto cleanup;
    }

    // Sort by inbound links * keyword count
    for (int i = 0; i < total_results - 1; i++) {
        for (int j = 0; j < total_results - i - 1; j++) {
            double score1 = results[j].count * (results[j].inbound + 1);
            double score2 = results[j + 1].count * (results[j + 1].inbound + 1);
            if (score2 > score1) {
                struct result temp = results[j];
                results[j] = results[j + 1];
                results[j + 1] = temp;
            }
        }
    }

    // Display top 6 (or fewer) with context
    printf("\nTop %d results with keyword '%s':\n", total_results, keyword);
    for (int i = 0; i < total_results; i++) {
        const char *source_str = results[i].source == 0 ? "dns" : "page";
        printf("[%d] %s <%s> (%d mentions, Inbound links: %d, Links: ", i + 1, results[i].identifier, source_str, results[i].count, results[i].inbound);
        if (results[i].inbound == 0 || !results[i].inbound_sources) {
            printf("None");
        } else {
            for (int j = 0; j < results[i].inbound && j < MAX_LINKS; j++) {
                if (results[i].inbound_sources[j]) {
                    printf("<link>%s</link>", results[i].inbound_sources[j]);
                    if (j < results[i].inbound - 1) printf(", ");
                }
            }
        }
        printf(")\n");
        // Display path for pages
        if (results[i].source == 2) {
            char *path = extract_path(results[i].identifier);
            if (path) {
                printf("%s\n", results[i].identifier);
                free(path);
            }
        }
        // Display context if available (text mode only)
        if (!save_html && results[i].context && results[i].context[0]) {
            // Truncate context for display (first 100 chars or first instance)
            char display_context[101];
            strncpy(display_context, results[i].context, 100);
            display_context[100] = '\0';
            char *newline = strchr(display_context, '\n');
            if (newline) *newline = '\0';
            printf("%s...\n", display_context);
        }
        printf("\n");
    }

    // Prompt user for selection
    int choice = -1;
    printf("Enter index (1-%d) to save and display, or 0 to exit: ", total_results);
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Invalid input\n");
        choice = 0;
    }
    while (getchar() != '\n'); // Clear input buffer
    if (choice < 1 || choice > total_results) {
        printf("Exiting without saving\n");
        goto cleanup;
    }

    // Save and display selected result
    int selected = choice - 1;
    char out_path[MAX_PATH];
    snprintf(out_path, MAX_PATH, "%s/%s_1.%s", sub_dir_path, safe_keyword, file_type);
    long size = save_content(results[selected].text, out_path);
    if (size >= 0) {
        printf("\nSaved %ld bytes to %s\n", size, out_path);
        printf("\nContent from %s:\n", results[selected].identifier);
        printf("%.1000s\n", results[selected].text); // Limit display

        // Save summary.txt with keyword context
        if (!save_html && results[selected].context) {
            char summary_path[MAX_PATH];
            snprintf(summary_path, MAX_PATH, "%s/%s_summary.txt", sub_dir_path, safe_keyword);
            long summary_size = save_content(results[selected].context, summary_path);
            if (summary_size >= 0) {
                printf("Saved %ld bytes to %s\n", summary_size, summary_path);
            } else {
                fprintf(stderr, "Failed to save summary content to %s\n", summary_path);
            }
        }
    } else {
        fprintf(stderr, "Failed to save content\n");
    }

cleanup:
    // Cleanup
    for (int i = 0; i < MAX_DOMAIN_HITS; i++) {
        if (domains_list[i]) free(domains_list[i]);
        if (domains_texts[i]) free(domains_texts[i]);
        if (domains_inbound_sources[i]) {
            for (int j = 0; j < MAX_LINKS; j++) {
                if (domains_inbound_sources[i][j]) free(domains_inbound_sources[i][j]);
            }
            free(domains_inbound_sources[i]);
        }
    }
    for (int i = 0; i < page_count; i++) {
        if (pages_list[i]) free(pages_list[i]);
        if (pages_texts[i]) free(pages_texts[i]);
        if (pages_inbound_sources[i]) {
            for (int j = 0; j < MAX_LINKS; j++) {
                if (pages_inbound_sources[i][j]) free(pages_inbound_sources[i][j]);
            }
            free(pages_inbound_sources[i]);
        }
    }
    for (int i = 0; i < all_pages_count; i++) {
        if (all_pages[i]) free(all_pages[i]);
        for (int j = 0; j < link_counts[i] && j < MAX_LINKS; j++) {
            if (all_links[i * MAX_LINKS + j]) free(all_links[i * MAX_LINKS + j]);
        }
        if (inbound_sources[i]) {
            for (int j = 0; j < inbound_counts[i] && j < MAX_LINKS; j++) {
                if (inbound_sources[i][j]) free(inbound_sources[i][j]);
            }
            free(inbound_sources[i]);
        }
    }
    for (int i = 0; i < total_results; i++) {
        if (results[i].context) free(results[i].context);
    }
    for (int i = 0; i < domain_list_count; i++) {
        if (dns_domains[i]) free(dns_domains[i]);
    }
    if (dns_domains) free(dns_domains);

    return size < 0;
}
