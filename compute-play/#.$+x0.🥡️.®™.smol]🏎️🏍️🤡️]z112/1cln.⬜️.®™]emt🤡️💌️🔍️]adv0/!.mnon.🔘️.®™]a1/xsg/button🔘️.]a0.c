#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_PATH 256
#define MAX_EXECS 100
#define LOG_FILE "log.txt"
#define LOCATIONS_FILE "locations.txt"

pid_t child_pids[MAX_EXECS];
int child_count = 0;
volatile sig_atomic_t running = 1;

void signal_handler(int sig) {
    running = 0;
    for (int i = 0; i < child_count; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
        }
    }
}

void* launch_executable(void* arg) {
    char* path = (char*)arg;
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return NULL;
    }
    if (pid == 0) {
        char* argv[] = {path, NULL};
        execvp(path, argv);
        perror("execvp failed");
        exit(1);
    } else {
        child_pids[child_count++] = pid;
    }
    return NULL;
}

int main() {
    FILE* log_fp;
    FILE* loc_fp;
    char* exec_paths[MAX_EXECS];
    int exec_count = 0;
    char line[MAX_PATH];
    pthread_t threads[MAX_EXECS];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    log_fp = fopen(LOG_FILE, "w");
    if (log_fp == NULL) {
        perror("Failed to open log.txt");
        return 1;
    }
    fprintf(log_fp, "test1\n");
    fclose(log_fp);

    loc_fp = fopen(LOCATIONS_FILE, "r");
    if (loc_fp == NULL) {
        perror("Failed to open locations.txt");
        return 1;
    }

    while (exec_count < MAX_EXECS && fgets(line, MAX_PATH, loc_fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        exec_paths[exec_count] = malloc(strlen(line) + 1);
        if (exec_paths[exec_count] == NULL) {
            perror("Memory allocation failed");
            fclose(loc_fp);
            return 1;
        }
        strcpy(exec_paths[exec_count], line);
        exec_count++;
    }
    fclose(loc_fp);

    for (int i = 0; i < exec_count; i++) {
        if (pthread_create(&threads[i], NULL, launch_executable, exec_paths[i]) != 0) {
            perror("pthread_create failed");
            for (int j = 0; j <= i; j++) {
                free(exec_paths[j]);
            }
            return 1;
        }
    }

    while (running) {
        pause();
    }

    for (int i = 0; i < exec_count; i++) {
        pthread_join(threads[i], NULL);
        free(exec_paths[i]);
    }

    for (int i = 0; i < child_count; i++) {
        waitpid(child_pids[i], NULL, 0);
    }

    return 0;
}
