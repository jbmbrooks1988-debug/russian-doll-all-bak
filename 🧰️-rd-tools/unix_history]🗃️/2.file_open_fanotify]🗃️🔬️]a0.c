#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <time.h>
#include <string.h>
#include <errno.h>

int main() {
    int fd;
    char buffer[4096];
    char path_buf[256];
    char log_entry[512];
    char timestamp[64];
    FILE *log_file;
    time_t now;

    fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY);
    if (fd < 0) {
        printf("Error initializing fanotify: %s\n", strerror(errno));
        return 1;
    }

    if (fanotify_mark(fd, FAN_MARK_ADD | FAN_MARK_MOUNT, FAN_OPEN, AT_FDCWD, "/") < 0) {
        printf("Error marking filesystem: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    while (1) {
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if (len < 0) {
            printf("Error reading fanotify events: %s\n", strerror(errno));
            continue;
        }

        char *ptr = buffer;
        while (ptr < buffer + len) {
            struct fanotify_event_metadata *event = (struct fanotify_event_metadata *)ptr;
            if (event->vers != FANOTIFY_METADATA_VERSION) {
                printf("Invalid fanotify event version\n");
                break;
            }

            int path_fd = open("/proc/self/fdinfo", O_RDONLY);
            if (path_fd >= 0) {
                snprintf(path_buf, sizeof(path_buf), "/proc/self/fd/%d", event->fd);
                ssize_t path_len = readlink(path_buf, path_buf, sizeof(path_buf) - 1);
                if (path_len > 0) {
                    path_buf[path_len] = '\0';
                    time(&now);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
                    snprintf(log_entry, sizeof(log_entry), "[%s] File opened: %s\n", timestamp, path_buf);

                    log_file = fopen("cpu_history.txt", "a");
                    if (log_file) {
                        fputs(log_entry, log_file);
                        fclose(log_file);
                    }
                }
                close(path_fd);
            }
            close(event->fd);
            ptr += event->event_len;
        }
    }

    close(fd);
    return 0;
}
