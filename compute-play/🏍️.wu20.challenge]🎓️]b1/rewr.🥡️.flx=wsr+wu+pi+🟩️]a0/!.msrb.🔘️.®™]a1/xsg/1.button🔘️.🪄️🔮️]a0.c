#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MAX_PATH 256
#define MAX_EXECUTABLES 10

pid_t child_pids[MAX_EXECUTABLES];
int child_count = 0;

void signal_handler(int sig) {
    for (int i = 0; i < child_count; i++) {
        kill(child_pids[i], SIGTERM);
    }
    exit(0);
}

void *run_executable(void *arg) {
    char *cmd = (char *)arg;
    pid_t pid = fork();
    
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        exit(1);
    } else if (pid > 0) {
        child_pids[child_count++] = pid;
    }
    
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    printf("test1\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    FILE *fp = fopen("locations.txt", "r");
    if (!fp) {
        printf("Error: Cannot open locations.txt\n");
        exit(1);
    }
    
    char line[MAX_PATH];
    char *executables[MAX_EXECUTABLES];
    int exec_count = 0;
    
    while (fgets(line, MAX_PATH, fp) && exec_count < MAX_EXECUTABLES) {
        line[strcspn(line, "\n")] = 0;
        executables[exec_count] = strdup(line);
        exec_count++;
    }
    fclose(fp);
    
    pthread_t threads[MAX_EXECUTABLES];
    
    for (int i = 0; i < exec_count; i++) {
        int rc = pthread_create(&threads[i], NULL, run_executable, (void *)executables[i]);
        if (rc) {
            printf("Error: pthread_create failed for %s\n", executables[i]);
            exit(1);
        }
    }
    
    for (int i = 0; i < exec_count; i++) {
        pthread_join(threads[i], NULL);
        free(executables[i]);
    }
    
    pthread_exit(NULL);
    return 0;
}
