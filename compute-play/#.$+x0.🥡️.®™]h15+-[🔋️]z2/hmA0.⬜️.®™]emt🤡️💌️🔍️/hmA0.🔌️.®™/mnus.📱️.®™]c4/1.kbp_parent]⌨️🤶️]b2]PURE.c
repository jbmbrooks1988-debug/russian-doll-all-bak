#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int pipefd[2];
    pid_t pid;
    char buf[1024];
    int fd;
    ssize_t nread;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        exit(1);
    }

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) { /* Child */
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(pipefd[0]);
        execlp(argv[1], argv[1], NULL);
        perror("execlp");
        exit(1);
    } else { /* Parent */
        close(pipefd[0]);
        fd = open("kbd.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            exit(1);
        }

        while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            if (write(pipefd[1], buf, nread) == -1) {
                perror("write to pipe");
                exit(1);
            }
            if (write(fd, buf, nread) == -1) {
                perror("write to file");
                exit(1);
            }
        }

        close(pipefd[1]);
        close(fd);
        wait(NULL);
    }

    return 0;
}
