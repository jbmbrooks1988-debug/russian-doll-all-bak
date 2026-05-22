/* win_posix_shim.h - Minimal POSIX compatibility for Windows TPMOS build */
#ifndef WIN_POSIX_SHIM_H
#define WIN_POSIX_SHIM_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

/* Networking */
#define close closesocket
static inline void win_init_sockets() {
    static int initialized = 0;
    if (!initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
        initialized = 1;
    }
}
#undef socket
#define socket(a,b,c) (win_init_sockets(), socket(a,b,c))

/* File System */
#define mkdir(path, mode) _mkdir(path)

/* Strings */
#define strcasestr(s, find) win_strcasestr(s, find)
static inline char* win_strcasestr(const char* s, const char* find) {
    char c, sc;
    size_t len;
    if ((c = *find++) != 0) {
        c = (char)tolower((unsigned char)c);
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0) return (NULL);
            } while ((char)tolower((unsigned char)sc) != c);
        } while (strncasecmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

/* Processes / Signals */
#ifndef SIGKILL
#define SIGKILL SIGTERM
#endif
#ifndef SIGHUP
#define SIGHUP SIGTERM
#endif
#define getpid _getpid
#define kill(pid, sig) (0)
#define WIFEXITED(status) (1)
#define WEXITSTATUS(status) (status)
#define WIFSIGNALED(status) (0)
#define WTERMSIG(status) (0)
#define WNOHANG 1

static inline int waitpid(intptr_t pid, int *status, int options) {
    if (pid <= 0) return 0;
    if (options & WNOHANG) {
        DWORD exitCode;
        if (GetExitCodeProcess((HANDLE)pid, &exitCode)) {
            if (exitCode == STILL_ACTIVE) return 0;
            if (status) *status = (int)exitCode;
            return (int)pid;
        }
        return -1;
    }
    return (int)_cwait(status, pid, _WAIT_CHILD);
}

/* asprintf shim (Safe Implementation) */
static inline int asprintf(char **strp, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = _vscprintf(fmt, args);
    va_end(args);
    if (len < 0) return -1;
    *strp = (char *)malloc(len + 1);
    if (!*strp) return -1;
    va_start(args, fmt);
    int r = vsnprintf(*strp, len + 1, fmt, args);
    va_end(args);
    return r;
}

/* strptime shim (partial) */
static inline char* strptime(const char* buf, const char* fmt, struct tm* tm) {
    (void)fmt; /* Very limited implementation - assumes YYYY-MM-DD */
    if (sscanf(buf, "%d-%d-%d", &tm->tm_year, &tm->tm_mon, &tm->tm_mday) == 3) {
        tm->tm_year -= 1900;
        tm->tm_mon -= 1;
        return (char*)(buf + 10);
    }
    return NULL;
}

/* getline shim for MinGW */
#include <errno.h>
#ifndef _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif
static inline ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (lineptr == NULL || n == NULL || stream == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = (char*)malloc(*n);
        if (*lineptr == NULL) return -1;
    }
    int c;
    size_t pos = 0;
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            size_t new_n = *n * 2;
            char *new_ptr = (char*)realloc(*lineptr, new_n);
            if (new_ptr == NULL) return -1;
            *lineptr = new_ptr;
            *n = new_n;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == '\n') break;
    }
    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}

#endif /* _WIN32 */
#endif /* WIN_POSIX_SHIM_H */
