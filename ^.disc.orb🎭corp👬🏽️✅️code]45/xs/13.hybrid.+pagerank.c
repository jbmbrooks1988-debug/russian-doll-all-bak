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

// Debug logging
void log_pagerank_debug(const char *message) {
    FILE *fp = fopen("pagerank_debug.txt", "a");
    if (!fp) {
        fprintf(stderr, "Failed to open pagerank_debug.txt: %s\n", strerror(errno));
        return;
    }
    fprintf(fp, "[%ld] %s\n", time(NULL), message);
    fclose(fp);
}

// Save content to file
long save_content(const char *content, const char *path) {
    if (!content || !path) {
        log_pagerank_debug("Invalid content or path in save_content");
        return -1;
    }
    FILE *fp = fopen(path, "w");
    if (!fp) {
        char log[256];
        snprintf(log, sizeof(log), "Failed to open %s: %s", path, strerror(errno));
        log_pagerank_debug(log);
        return -1;
    }
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);
    if (written != len) {
        char log[256];
        snprintf(log, sizeof(log), "Incomplete write to %s: %zu of %zu bytes", path, written, len);
        log_pagerank_debug(log);
        return -1;
    }
    return written;
}

// Send HTTP/HTTPS GET request
int send_http_get(const char *host, const char *path, char *response, size_t max_size, int use_ssl, char *redirect_url, size_t redirect_size) {
    if (!host || !path || !response || !redirect_url) {
        log_pagerank_debug("Invalid input parameters in send_http_get");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        char log[256];
        snprintf(log, sizeof(log), "Socket creation failed: %s", strerror(errno));
        log_pagerank_debug(log);
        return -1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(use_ssl ? HTTPS_PORT : HTTP_PORT);

    struct hostent *server = gethostbyname(host);
    if (!server) {
        char log[256];
        snprintf(log, sizeof(log), "Host resolution failed for %s", host);
        log_pagerank_debug(log);
        close(sock);
        return -1;
    }
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        char log[256];
        snprintf(log, sizeof(log), "Connection failed to %s:%d: %s", host, use_ssl ? HTTPS_PORT : HTTP_PORT, strerror(errno));
        log_pagerank_debug(log);
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
            log_pagerank_debug("SSL_CTX_new failed");
            close(sock);
            return -1;
        }
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
        const char *ciphers = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";
        if (SSL_CTX_set_cipher_list(ctx, ciphers) != 1) {
            log_pagerank_debug("Failed to set cipher list");
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
        ssl = SSL_new(ctx);
        if (!ssl) {
            log_pagerank_debug("SSL_new failed");
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
        if (SSL_set_fd(ssl, sock) == 0) {
            log_pagerank_debug("SSL_set_fd failed");
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
        if (SSL_connect(ssl) != 1) {
            char log[256];
            snprintf(log, sizeof(log), "SSL_connect failed: %s", ERR_error_string(ERR_get_error(), NULL));
            log_pagerank_debug(log);
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
        char log[256];
        snprintf(log, sizeof(log), "Request buffer overflow for %s", host);
        log_pagerank_debug(log);
        if (use_ssl) {
            SSL_free(ssl);
            SSL_CTX_free(ctx);
        }
        close(sock);
        return -1;
    }

    if (use_ssl) {
        if (SSL_write(ssl, request, strlen(request)) <= 0) {
            char log[256];
            snprintf(log, sizeof(log), "SSL_write failed: %s", ERR_error_string(ERR_get_error(), NULL));
            log_pagerank_debug(log);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sock);
            return -1;
        }
    } else {
        if (send(sock, request, strlen(request), 0) < 0) {
            char log[256];
            snprintf(log, sizeof(log), "Send failed: %s", strerror(errno));
            log_pagerank_debug(log);
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
            char log[256];
            snprintf(log, sizeof(log), "Response too large for buffer from %s", host);
            log_pagerank_debug(log);
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
                    char log[256];
                    snprintf(log, sizeof(log), "Redirect detected to %s", redirect_url);
                    log_pagerank_debug(log);
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
        log_pagerank_debug("Invalid html or keyword in count_keyword");
        return 0;
    }
    int count = 0;
    char lower_keyword[MAX_URL];
    strncpy(lower_keyword, keyword, MAX_URL - 1);
    lower_keyword[MAX_URL - 1] = '\0';
    to_lowercase(lower_keyword);

    char *lower_html = strdup(html);
    if (!lower_html) {
        log_pagerank_debug("Failed to allocate lower_html");
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
        log_pagerank_debug("Invalid html or text buffer in extract_text");
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
        } else if (!in_tag && !in_script && text_len < max_size - 1) {
            text[text_len++] = *p;
        }
    }
    text[text_len] = '\0';
    return text_len;
}

// Extract keyword context
void extract_keyword_context(const char *text, const char *keyword, char *context, size_t max_size) {
    if (!text || !keyword || !context) {
        log_pagerank_debug("Invalid input in extract_keyword_context");
        return;
    }
    char lower_keyword[MAX_URL];
    strncpy(lower_keyword, keyword, MAX_URL - 1);
    lower_keyword[MAX_URL - 1] = '\0';
    to_lowercase(lower_keyword);

    char *lower_text = strdup(text);
    if (!lower_text) {
        log_pagerank_debug("Failed to allocate lower_text in extract_keyword_context");
        return;
    }
    to_lowercase(lower_text);

    context[0] = '\0';
    char *ptr = lower_text;
    int word_count = 0;
    char *start = NULL;
    while ((ptr = strstr(ptr, lower_keyword)) && word_count < CONTEXT_WORDS) {
        start = ptr;
        while (start > lower_text && *(start - 1) != ' ' && *(start - 1) != '\n') {
            start--;
        }
        ptr += strlen(lower_keyword);
        while (*ptr && *ptr != ' ' && *ptr != '\n') {
            ptr++;
        }
        word_count++;
    }
    if (start) {
        char *end = ptr;
        int i = 0;
        while (*end && *end != '\n' && i < 10) {
            end++;
            if (*end == ' ') i++;
        }
        size_t len = end - start;
        if (len > max_size - 1) len = max_size - 1;
        strncpy(context, start, len);
        context[len] = '\0';
    }
    free(lower_text);
}

// Parse links from HTML
void parse_links(const char *html, char **links, int *link_count, size_t max_links) {
    if (!html || !links) {
        log_pagerank_debug("Invalid html or links array in parse_links");
        return;
    }
    *link_count = 0;
    char *ptr = (char *)html;
    while (*link_count < max_links) {
        ptr = strstr(ptr, "href=\"");
        if (!ptr) break;
        ptr += 6;
        char *end = strchr(ptr, '\"');
        if (!end) break;
        int len = end - ptr;
        if (len > 0 && len < MAX_URL) {
            links[*link_count] = malloc(len + 1);
            if (links[*link_count]) {
                strncpy(links[*link_count], ptr, len);
                links[*link_count][len] = '\0';
                (*link_count)++;
            }
        }
        ptr = end + 1;
    }
}

// Count inbound links
void count_inbound_links(char **pages, int page_count, char **all_links, int *link_counts, int *inbound_counts, char **inbound_sources, size_t max_links) {
    for (int i = 0; i < page_count; i++) {
        inbound_counts[i] = 0;
        if (!pages[i]) continue;
        for (int j = 0; j < page_count; j++) {
            if (i == j || !pages[j]) continue;
            for (int k = 0; k < link_counts[j] && k < max_links; k++) {
                if (all_links[j * max_links + k] && strcmp(all_links[j * max_links + k], pages[i]) == 0) {
                    inbound_counts[i]++;
                    if (inbound_counts[i] <= max_links && inbound_sources[i]) {
                        inbound_sources[i][inbound_counts[i] - 1] = strdup(pages[j]);
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        log_pagerank_debug("Usage: ./13.hybrid.+pagerank.+x <keyword> [txt|html]");
        return 1;
    }

    const char *keyword = argv[1];
    int save_html = (argc > 2 && strcmp(argv[2], "html") == 0);
    const char *file_type = save_html ? "html" : "txt";
    char safe_keyword[MAX_URL];
    sanitize_keyword(keyword, safe_keyword, MAX_URL);

    // Create text-dl directory
    char sub_dir_path[MAX_PATH];
    snprintf(sub_dir_path, MAX_PATH, "text-dl");
    if (mkdir(sub_dir_path, 0755) != 0 && errno != EEXIST) {
        char log[256];
        snprintf(log, sizeof(log), "Failed to create directory %s: %s", sub_dir_path, strerror(errno));
        log_pagerank_debug(log);
        return 1;
    }

    // Load domains
    char **dns_domains = NULL;
    int domain_list_count = 0;
    FILE *dns_fp = fopen("dns_domains.txt", "r");
    if (!dns_fp) {
        char log[256];
        snprintf(log, sizeof(log), "Failed to open dns_domains.txt: %s", strerror(errno));
        log_pagerank_debug(log);
        return 1;
    }
    char line[MAX_URL];
    while (fgets(line, sizeof(line), dns_fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 0) {
            dns_domains = realloc(dns_domains, (domain_list_count + 1) * sizeof(char *));
            dns_domains[domain_list_count] = strdup(line);
            domain_list_count++;
        }
    }
    fclose(dns_fp);
    char log[256];
    snprintf(log, sizeof(log), "Loaded %d domains from dns_domains.txt", domain_list_count);
    log_pagerank_debug(log);

    // Process domains
    char *domains_list[MAX_DOMAIN_HITS] = {0};
    char *domains_texts[MAX_DOMAIN_HITS] = {0};
    int domains_counts[MAX_DOMAIN_HITS] = {0};
    int domains_inbound[MAX_DOMAIN_HITS] = {0};
    char **domains_inbound_sources[MAX_DOMAIN_HITS] = {0};
    int domain_count = 0;

    for (int i = 0; i < domain_list_count && domain_count < MAX_DOMAIN_HITS; i++) {
        char *domain = dns_domains[i];
        if (!domain) continue;
        char html[MAX_HTML] = {0};
        char redirect_url[MAX_URL] = {0};
        int use_ssl = strstr(domain, "https://") != NULL;
        char *path = extract_path(domain);
        char *host = extract_domain(domain);
        if (!host) {
            free(path);
            continue;
        }

        snprintf(log, sizeof(log), "Fetching %s", domain);
        log_pagerank_debug(log);
        int size = send_http_get(host, path ? path : "/", html, MAX_HTML, use_ssl, redirect_url, MAX_URL);
        if (size == -2) {
            snprintf(log, sizeof(log), "Redirect detected to %s", redirect_url);
            log_pagerank_debug(log);
            free(host);
            free(path);
            char *new_domain = strdup(redirect_url);
            if (!new_domain) continue;
            for (int j = 0; j < domain_count; j++) {
                if (domains_list[j] && strcmp(domains_list[j], new_domain) == 0) {
                    free(new_domain);
                    new_domain = NULL;
                    break;
                }
            }
            if (new_domain) {
                dns_domains[i] = new_domain;
                i--;
            }
            continue;
        }
        if (size > 0) {
            domains_list[domain_count] = strdup(domain);
            domains_texts[domain_count] = malloc(MAX_TEXT);
            if (!domains_list[domain_count] || !domains_texts[domain_count]) {
                free(host);
                free(path);
                free(domains_list[domain_count]);
                free(domains_texts[domain_count]);
                continue;
            }
            long text_size = extract_text(html, domains_texts[domain_count], MAX_TEXT);
            if (text_size > 0) {
                domains_counts[domain_count] = count_keyword(domains_texts[domain_count], keyword);
                if (domains_counts[domain_count] > 0) {
                    snprintf(log, sizeof(log), "Found domain: %s (%d keyword matches)", domains_list[domain_count], domains_counts[domain_count]);
                    log_pagerank_debug(log);
                    domains_inbound_sources[domain_count] = calloc(MAX_LINKS, sizeof(char *));
                    if (!domains_inbound_sources[domain_count]) {
                        free(domains_list[domain_count]);
                        free(domains_texts[domain_count]);
                        continue;
                    }
                    domain_count++;
                } else {
                    snprintf(log, sizeof(log), "No keyword matches for %s", domain);
                    log_pagerank_debug(log);
                    free(domains_list[domain_count]);
                    free(domains_texts[domain_count]);
                }
            } else {
                free(domains_list[domain_count]);
                free(domains_texts[domain_count]);
            }
        }
        free(host);
        free(path);
    }

    // Process linked pages
    char *pages_list[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char *pages_texts[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int pages_counts[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int pages_inbound[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char **pages_inbound_sources[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int page_count = 0;

    char *all_pages[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char *all_links[MAX_DOMAINS * MAX_PAGE_HITS * MAX_LINKS] = {0};
    int link_counts[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int inbound_counts[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    char **inbound_sources[MAX_DOMAINS * MAX_PAGE_HITS] = {0};
    int all_pages_count = 0;

    for (int i = 0; i < domain_count; i++) {
        char *links[MAX_LINKS] = {0};
        int link_count = 0;
        parse_links(domains_texts[i], links, &link_count, MAX_LINKS);
        for (int j = 0; j < link_count && page_count < MAX_DOMAINS * MAX_PAGE_HITS; j++) {
            char *link = links[j];
            if (!link) continue;
            if (strncmp(link, "http://", 7) != 0 && strncmp(link, "https://", 8) != 0) {
                free(link);
                continue;
            }
            char *link_domain = extract_domain(link);
            char *link_path = extract_path(link);
            if (!link_domain || !link_path) {
                free(link);
                free(link_domain);
                free(link_path);
                continue;
            }
            char html[MAX_HTML] = {0};
            char redirect_url[MAX_URL] = {0};
            int use_ssl = strstr(link, "https://") != NULL;
            snprintf(log, sizeof(log), "Fetching linked page %s", link);
            log_pagerank_debug(log);
            int size = send_http_get(link_domain, link_path, html, MAX_HTML, use_ssl, redirect_url, MAX_URL);
            if (size == -2) {
                snprintf(log, sizeof(log), "Redirect detected to %s for linked page", redirect_url);
                log_pagerank_debug(log);
                free(link);
                free(link_domain);
                free(link_path);
                continue;
            }
            if (size > 0) {
                char *page_text = malloc(MAX_TEXT);
                if (!page_text) {
                    free(link);
                    free(link_domain);
                    free(link_path);
                    continue;
                }
                long text_size = extract_text(html, page_text, MAX_TEXT);
                if (text_size > 0) {
                    int page_count_keywords = count_keyword(page_text, keyword);
                    if (page_count_keywords > 0) {
                        pages_list[page_count] = strdup(link);
                        pages_texts[page_count] = page_text;
                        pages_counts[page_count] = page_count_keywords;
                        pages_inbound_sources[page_count] = calloc(MAX_LINKS, sizeof(char *));
                        if (!pages_list[page_count] || !pages_inbound_sources[page_count]) {
                            free(pages_list[page_count]);
                            free(pages_texts[page_count]);
                            free(pages_inbound_sources[page_count]);
                            free(link);
                            free(link_domain);
                            free(link_path);
                            continue;
                        }
                        all_pages[all_pages_count] = strdup(link_domain);
                        link_counts[all_pages_count] = 0;
                        inbound_sources[all_pages_count] = calloc(MAX_LINKS, sizeof(char *));
                        all_pages_count++;
                        snprintf(log, sizeof(log), "Found linked page: %s (%d keyword matches)", pages_list[page_count], page_count_keywords);
                        log_pagerank_debug(log);
                        page_count++;
                    } else {
                        snprintf(log, sizeof(log), "No keyword matches for linked page %s", link);
                        log_pagerank_debug(log);
                        free(page_text);
                    }
                } else {
                    free(page_text);
                }
            }
            free(link);
            free(link_domain);
            free(link_path);
        }
        for (int j = 0; j < link_count; j++) {
            if (links[j]) free(links[j]);
        }
    }

    // Count inbound links
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

    snprintf(log, sizeof(log), "Total domains found: %d", domain_count);
    log_pagerank_debug(log);
    snprintf(log, sizeof(log), "Total linked pages found: %d", page_count);
    log_pagerank_debug(log);

    // Merge results
    struct result {
        char *identifier;
        char *text;
        int count;
        int inbound;
        char **inbound_sources;
        int source; // 0=dns, 2=page
        char *context;
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
        snprintf(log, sizeof(log), "No domains or pages with keyword matches for '%s'", keyword);
        log_pagerank_debug(log);
        FILE *results_fp = fopen("dns_results.txt", "w");
        if (results_fp) {
            fprintf(results_fp, "No results found for '%s'\n", keyword);
            fclose(results_fp);
        }
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

    // Write results to dns_results.txt
    FILE *results_fp = fopen("dns_results.txt", "w");
    if (!results_fp) {
        snprintf(log, sizeof(log), "Failed to open dns_results.txt: %s", strerror(errno));
        log_pagerank_debug(log);
        goto cleanup;
    }
    for (int i = 0; i < total_results; i++) {
        const char *source_str = results[i].source == 0 ? "dns" : "page";
        fprintf(results_fp, "%s (%d keyword matches, Inbound links: %d)\n", results[i].identifier, results[i].count, results[i].inbound);
        if (!save_html && results[i].context && results[i].context[0]) {
            char truncated_context[101];
            strncpy(truncated_context, results[i].context, 100);
            truncated_context[100] = '\0';
            char *newline = strchr(truncated_context, '\n');
            if (newline) *newline = '\0';
            fprintf(results_fp, "Text: %s...\n", truncated_context);
        }
    }
    fclose(results_fp);
    snprintf(log, sizeof(log), "Wrote %d results to dns_results.txt", total_results);
    log_pagerank_debug(log);

    // Automatically select top result (index 1)
    int selected = 0; // Index 0 is top result after sorting
    if (total_results > 0) {
        char out_path[MAX_PATH];
        snprintf(out_path, MAX_PATH, "%s/%s_1.%s", sub_dir_path, safe_keyword, file_type);
        long size = save_content(results[selected].text, out_path);
        if (size >= 0) {
            snprintf(log, sizeof(log), "Saved %ld bytes to %s", size, out_path);
            log_pagerank_debug(log);
        } else {
            snprintf(log, sizeof(log), "Failed to save content to %s", out_path);
            log_pagerank_debug(log);
        }

        // Save summary.txt with keyword context
        if (!save_html && results[selected].context) {
            char summary_path[MAX_PATH];
            snprintf(summary_path, MAX_PATH, "%s/%s_summary.txt", sub_dir_path, safe_keyword);
            long summary_size = save_content(results[selected].context, summary_path);
            if (summary_size >= 0) {
                snprintf(log, sizeof(log), "Saved %ld bytes to %s", summary_size, summary_path);
                log_pagerank_debug(log);
            } else {
                snprintf(log, sizeof(log), "Failed to save summary content to %s", summary_path);
                log_pagerank_debug(log);
            }
        }
    } else {
        snprintf(log, sizeof(log), "No results to save for '%s'", keyword);
        log_pagerank_debug(log);
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

    return total_results > 0 ? 0 : 1;
}
