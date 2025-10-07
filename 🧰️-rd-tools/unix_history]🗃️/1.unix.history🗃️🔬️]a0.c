#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

int main() {
    DIR *proc_dir;
    struct dirent *entry;
    char path[256];
    char exe_path[256];
    char log_entry[512];
    char timestamp[64];
    FILE *log_file;
    time_t now;

    while (1) {
        proc_dir = opendir("/proc");
        if (proc_dir == NULL) {
            printf("Error opening /proc\n");
            return 1;
        }

        while ((entry = readdir(proc_dir)) != NULL) {
            if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
                int pid = atoi(entry->d_name);
                snprintf(path, sizeof(path), "/proc/%s/exe", entry->d_name);
                ssize_t len = readlink(path, exe_path, sizeof(exe_path) - 1);
                if (len != -1) {
                    exe_path[len] = '\0';
                    time(&now);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
                    snprintf(log_entry, sizeof(log_entry), "[%s] PID: %d, Path: %s\n", timestamp, pid, exe_path);
                    
                    log_file = fopen("cpu_history.txt", "a");
                    if (log_file) {
                        fputs(log_entry, log_file);
                        fclose(log_file);
                    }
                }
            }
        }
        closedir(proc_dir);
        sleep(1);
    }
    return 0;
}
